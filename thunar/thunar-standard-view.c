/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-icon-renderer.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>
#include <thunar/thunar-renamer-dialog.h>
#include <thunar/thunar-simple-job.h>
#include <thunar/thunar-standard-view.h>
#include <thunar/thunar-standard-view-ui.h>
#include <thunar/thunar-templates-action.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-thumbnailer.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif



#define THUNAR_STANDARD_VIEW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_LOADING,
  PROP_DISPLAY_NAME,
  PROP_TOOLTIP_TEXT,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_STATUSBAR_TEXT,
  PROP_UI_MANAGER,
  PROP_ZOOM_LEVEL,
  N_PROPERTIES
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
  TARGET_TEXT_URI_LIST,
  TARGET_XDND_DIRECT_SAVE0,
  TARGET_NETSCAPE_URL,
};



static void                 thunar_standard_view_component_init             (ThunarComponentIface     *iface);
static void                 thunar_standard_view_navigator_init             (ThunarNavigatorIface     *iface);
static void                 thunar_standard_view_view_init                  (ThunarViewIface          *iface);
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
static void                 thunar_standard_view_reload                     (ThunarView               *view,
                                                                             gboolean                  reload_info);
static gboolean             thunar_standard_view_get_visible_range          (ThunarView               *view,
                                                                             ThunarFile              **start_file,
                                                                             ThunarFile              **end_file);
static void                 thunar_standard_view_scroll_to_file             (ThunarView               *view,
                                                                             ThunarFile               *file,
                                                                             gboolean                  select_file,
                                                                             gboolean                  use_align,
                                                                             gfloat                    row_align,
                                                                             gfloat                    col_align);
static gboolean             thunar_standard_view_delete_selected_files      (ThunarStandardView       *standard_view);
static GdkDragAction        thunar_standard_view_get_dest_actions           (ThunarStandardView       *standard_view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarFile              **file_return);
static ThunarFile          *thunar_standard_view_get_drop_file              (ThunarStandardView       *standard_view,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkTreePath             **path_return);
static void                 thunar_standard_view_merge_custom_actions       (ThunarStandardView       *standard_view,
                                                                             GList                    *selected_items);
static void                 thunar_standard_view_update_statusbar_text      (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_current_directory_destroy  (ThunarFile               *current_directory,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_current_directory_changed  (ThunarFile               *current_directory,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_empty_file   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_folder       (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_create_template     (GtkAction                *action,
                                                                             const ThunarFile         *file,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_properties          (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_cut                 (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_copy                (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_paste               (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_move_to_trash       (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_delete             (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_paste_into_folder   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_select_all_files    (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_select_by_pattern   (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_selection_invert    (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_duplicate           (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_make_link           (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_rename              (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_action_restore             (GtkAction                *action,
                                                                             ThunarStandardView       *standard_view);
static GClosure            *thunar_standard_view_new_files_closure          (ThunarStandardView       *standard_view,
                                                                             GtkWidget                *source_view);
static void                 thunar_standard_view_new_files                  (ThunarStandardView       *standard_view,
                                                                             GList                    *path_list);
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
static gboolean             thunar_standard_view_button_press_event         (GtkWidget                *view,
                                                                             GdkEventButton           *event,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_drop                  (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_received         (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_leave                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_drag_motion                (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             gint                      x,
                                                                             gint                      y,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_begin                 (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_get              (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             GtkSelectionData         *selection_data,
                                                                             guint                     info,
                                                                             guint                     timestamp,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_data_delete           (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_drag_end                   (GtkWidget                *view,
                                                                             GdkDragContext           *context,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_row_deleted                (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_select_after_row_deleted   (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_restore_selection_idle     (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_row_changed                (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             GtkTreeIter              *iter,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_rows_reordered             (ThunarListModel          *model,
                                                                             GtkTreePath              *path,
                                                                             GtkTreeIter              *iter,
                                                                             gpointer                  new_order,
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
static void                 thunar_standard_view_finished_thumbnailing      (ThunarThumbnailer        *thumbnailer,
                                                                             guint                     request,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_thumbnailing_destroyed     (gpointer                  data);
static void                 thunar_standard_view_cancel_thumbnailing        (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_schedule_thumbnail_timeout (ThunarStandardView       *standard_view);
static void                 thunar_standard_view_schedule_thumbnail_idle    (ThunarStandardView       *standard_view);
static gboolean             thunar_standard_view_request_thumbnails         (gpointer                  data);
static gboolean             thunar_standard_view_request_thumbnails_lazy    (gpointer                  data);
static void                 thunar_standard_view_thumbnail_mode_toggled     (ThunarStandardView       *standard_view,
                                                                             GParamSpec               *pspec,
                                                                             ThunarIconFactory        *icon_factory);
static void                 thunar_standard_view_scrolled                   (GtkAdjustment            *adjustment,
                                                                             ThunarStandardView       *standard_view);
static void                 thunar_standard_view_size_allocate              (ThunarStandardView       *standard_view,
                                                                             GtkAllocation            *allocation);



struct _ThunarStandardViewPrivate
{
  /* current directory of the view */
  ThunarFile             *current_directory;

  GtkAction              *action_create_folder;
  GtkAction              *action_create_document;
  GtkAction              *action_properties;
  GtkAction              *action_cut;
  GtkAction              *action_copy;
  GtkAction              *action_paste;
  GtkAction              *action_move_to_trash;
  GtkAction              *action_delete;
  GtkAction              *action_paste_into_folder;
  GtkAction              *action_duplicate;
  GtkAction              *action_make_link;
  GtkAction              *action_rename;
  GtkAction              *action_restore;

  /* history of the current view */
  ThunarHistory          *history;

  /* support for file manager extensions */
  ThunarxProviderFactory *provider_factory;

  /* zoom-level support */
  ThunarZoomLevel         zoom_level;

  /* scroll_to_file support */
  GHashTable             *scroll_to_files;

  /* statusbar */
  gchar                  *statusbar_text;
  guint                   statusbar_text_idle_id;

  /* custom menu actions support */
  GtkActionGroup         *custom_actions;
  gint                    custom_merge_id;

  /* right-click drag/popup support */
  GList                  *drag_g_file_list;
  guint                   drag_scroll_timer_id;
  guint                   drag_timer_id;
  gint                    drag_x;
  gint                    drag_y;

  /* drop site support */
  guint                   drop_data_ready : 1; /* whether the drop data was received already */
  guint                   drop_highlight : 1;
  guint                   drop_occurred : 1;   /* whether the data was dropped */
  GList                  *drop_file_list;      /* the list of URIs that are contained in the drop data */

  /* the "new-files" closure, which is used to select files whenever
   * new files are created by a ThunarJob associated with this view
   */
  GClosure               *new_files_closure;

  /* the "new-files" path list that was remembered in the closure callback
   * if the view is currently being loaded and as such the folder may
   * not have all "new-files" at hand. This list is used when the
   * folder tells that it's ready loading and the view will try again
   * to select exactly these files.
   */
  GList                  *new_files_path_list;

  /* scroll-to-file support */
  ThunarFile             *scroll_to_file;
  guint                   scroll_to_select : 1;
  guint                   scroll_to_use_align : 1;
  gfloat                  scroll_to_row_align;
  gfloat                  scroll_to_col_align;

  /* selected_files support */
  GList                  *selected_files;
  guint                   restore_selection_idle_id;

  /* support for generating thumbnails */
  ThunarThumbnailer      *thumbnailer;
  guint                   thumbnail_request;
  guint                   thumbnail_source_id;
  gboolean                thumbnailing_scheduled;

  /* file insert signal */
  gulong                  row_changed_id;

  /* Tree path for restoring the selection after selecting and
   * deleting an item */
  GtkTreePath            *selection_before_delete;
};



static const GtkActionEntry action_entries[] =
{
  { "file-context-menu", NULL, N_ ("File Context Menu"), NULL, NULL, NULL, },
  { "folder-context-menu", NULL, N_ ("Folder Context Menu"), NULL, NULL, NULL, },
  { "create-folder", "folder-new", N_ ("Create _Folder..."), "<control><shift>N", N_ ("Create an empty folder within the current folder"), G_CALLBACK (thunar_standard_view_action_create_folder), },
  { "properties", GTK_STOCK_PROPERTIES, N_ ("_Properties..."), "<alt>Return", N_ ("View the properties of the selected file"), G_CALLBACK (thunar_standard_view_action_properties), },
  { "cut", GTK_STOCK_CUT, N_ ("Cu_t"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_cut), },
  { "copy", GTK_STOCK_COPY, N_ ("_Copy"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_copy), },
  { "paste", GTK_STOCK_PASTE, N_ ("_Paste"), NULL, N_ ("Move or copy files previously selected by a Cut or Copy command"), G_CALLBACK (thunar_standard_view_action_paste), },
  { "move-to-trash", THUNAR_STOCK_TRASH_FULL, N_ ("Mo_ve to Trash"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_move_to_trash), },
  { "delete", GTK_STOCK_DELETE, N_ ("_Delete"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_delete), },
  { "paste-into-folder", GTK_STOCK_PASTE, N_ ("Paste Into Folder"), NULL, N_ ("Move or copy files previously selected by a Cut or Copy command into the selected folder"), G_CALLBACK (thunar_standard_view_action_paste_into_folder), },
  { "select-all-files", NULL, N_ ("Select _all Files"), NULL, N_ ("Select all files in this window"), G_CALLBACK (thunar_standard_view_action_select_all_files), },
  { "select-by-pattern", NULL, N_ ("Select _by Pattern..."), "<control>S", N_ ("Select all files that match a certain pattern"), G_CALLBACK (thunar_standard_view_action_select_by_pattern), },
  { "invert-selection", NULL, N_ ("_Invert Selection"), NULL, N_ ("Select all and only the items that are not currently selected"), G_CALLBACK (thunar_standard_view_action_selection_invert), },
  { "duplicate", NULL, N_ ("Du_plicate"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_duplicate), },
  { "make-link", NULL, N_ ("Ma_ke Link"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_make_link), },
  { "rename", NULL, N_ ("_Rename..."), "F2", NULL, G_CALLBACK (thunar_standard_view_action_rename), },
  { "restore", NULL, N_ ("_Restore"), NULL, NULL, G_CALLBACK (thunar_standard_view_action_restore), },
};

/* Target types for dragging from the view */
static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};

/* Target types for dropping to the view */
static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
  { "XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0, },
  { "_NETSCAPE_URL", 0, TARGET_NETSCAPE_URL, },
};



static guint       standard_view_signals[LAST_SIGNAL];
static GParamSpec *standard_view_props[N_PROPERTIES] = { NULL, };



G_DEFINE_ABSTRACT_TYPE_WITH_CODE (ThunarStandardView, thunar_standard_view, GTK_TYPE_SCROLLED_WINDOW,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_standard_view_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_standard_view_component_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_VIEW, thunar_standard_view_view_init))



static void
thunar_standard_view_class_init (ThunarStandardViewClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GtkBindingSet  *binding_set;
  GObjectClass   *gobject_class;
  gpointer        g_iface;

  g_type_class_add_private (klass, sizeof (ThunarStandardViewPrivate));

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
   *
   * Override property to set the property as writable
   * for the binding.
   **/
  standard_view_props[PROP_LOADING] =
      g_param_spec_override ("loading",
                             g_param_spec_boolean ("loading",
                                                   "loading",
                                                   "loading",
                                                   FALSE,
                                                   EXO_PARAM_READWRITE));

  /**
   * ThunarStandardView:display-name:
   *
   * Display name of the current directory, for label text
   **/
  standard_view_props[PROP_DISPLAY_NAME] =
      g_param_spec_string ("display-name",
                           "display-name",
                           "display-name",
                           NULL,
                           EXO_PARAM_READABLE);

  /**
   * ThunarStandardView:parse-name:
   *
   * Full parsed name of the current directory, for label tooltip
   **/
  standard_view_props[PROP_TOOLTIP_TEXT] =
      g_param_spec_string ("tooltip-text",
                           "tooltip-text",
                           "tooltip-text",
                           NULL,
                           EXO_PARAM_READABLE);

  /* override ThunarComponent's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_COMPONENT);
  standard_view_props[PROP_SELECTED_FILES] =
      g_param_spec_override ("selected-files",
                             g_object_interface_find_property (g_iface, "selected-files"));

  standard_view_props[PROP_UI_MANAGER] =
      g_param_spec_override ("ui-manager",
                             g_object_interface_find_property (g_iface, "ui-manager"));

  /* override ThunarNavigator's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_NAVIGATOR);
  standard_view_props[PROP_CURRENT_DIRECTORY] =
      g_param_spec_override ("current-directory",
                             g_object_interface_find_property (g_iface, "current-directory"));

  /* override ThunarView's properties */
  g_iface = g_type_default_interface_peek (THUNAR_TYPE_VIEW);
  standard_view_props[PROP_STATUSBAR_TEXT] =
      g_param_spec_override ("statusbar-text",
                             g_object_interface_find_property (g_iface, "statusbar-text"));

  standard_view_props[PROP_SHOW_HIDDEN] =
      g_param_spec_override ("show-hidden",
                             g_object_interface_find_property (g_iface, "show-hidden"));

  standard_view_props[PROP_ZOOM_LEVEL] =
      g_param_spec_override ("zoom-level",
                             g_object_interface_find_property (g_iface, "zoom-level"));

  /* install all properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, standard_view_props);

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
  gtk_binding_entry_add_signal (binding_set, GDK_Delete, GDK_SHIFT_MASK, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, 0, "delete-selected-files", 0);
  gtk_binding_entry_add_signal (binding_set, GDK_KP_Delete, GDK_SHIFT_MASK, "delete-selected-files", 0);
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

  /* allocate the scroll_to_files mapping (directory GFile -> first visible child GFile) */
  standard_view->priv->scroll_to_files = g_hash_table_new_full (g_file_hash, (GEqualFunc) g_file_equal, g_object_unref, g_object_unref);

  /* grab a reference on the preferences */
  standard_view->preferences = thunar_preferences_get ();

  /* grab a reference on the provider factory */
  standard_view->priv->provider_factory = thunarx_provider_factory_get_default ();

  /* create a thumbnailer */
  standard_view->priv->thumbnailer = thunar_thumbnailer_get ();
  g_signal_connect (G_OBJECT (standard_view->priv->thumbnailer), "request-finished", G_CALLBACK (thunar_standard_view_finished_thumbnailing), standard_view);
  standard_view->priv->thumbnailing_scheduled = FALSE;

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
  standard_view->priv->action_move_to_trash = gtk_action_group_get_action (standard_view->action_group, "move-to-trash");
  standard_view->priv->action_delete = gtk_action_group_get_action (standard_view->action_group, "delete");
  standard_view->priv->action_paste_into_folder = gtk_action_group_get_action (standard_view->action_group, "paste-into-folder");
  standard_view->priv->action_duplicate = gtk_action_group_get_action (standard_view->action_group, "duplicate");
  standard_view->priv->action_make_link = gtk_action_group_get_action (standard_view->action_group, "make-link");
  standard_view->priv->action_rename = gtk_action_group_get_action (standard_view->action_group, "rename");
  standard_view->priv->action_restore = gtk_action_group_get_action (standard_view->action_group, "restore");

  /* add the "Create Document" sub menu action */
  standard_view->priv->action_create_document = thunar_templates_action_new ("create-document", _("Create _Document"));
  g_signal_connect (G_OBJECT (standard_view->priv->action_create_document), "create-empty-file",
                    G_CALLBACK (thunar_standard_view_action_create_empty_file), standard_view);
  g_signal_connect (G_OBJECT (standard_view->priv->action_create_document), "create-template",
                    G_CALLBACK (thunar_standard_view_action_create_template), standard_view);
  gtk_action_group_add_action (standard_view->action_group, standard_view->priv->action_create_document);
  g_object_unref (G_OBJECT (standard_view->priv->action_create_document));

  /* setup the history support */
  standard_view->priv->history = g_object_new (THUNAR_TYPE_HISTORY, "action-group", standard_view->action_group, NULL);
  g_signal_connect_swapped (G_OBJECT (standard_view->priv->history), "change-directory", G_CALLBACK (thunar_navigator_change_directory), standard_view);

  /* setup the list model */
  standard_view->model = thunar_list_model_new ();
  g_signal_connect (G_OBJECT (standard_view->model), "row-deleted", G_CALLBACK (thunar_standard_view_row_deleted), standard_view);
  g_signal_connect_after (G_OBJECT (standard_view->model), "row-deleted", G_CALLBACK (thunar_standard_view_select_after_row_deleted), standard_view);
  standard_view->priv->row_changed_id = g_signal_connect (G_OBJECT (standard_view->model), "row-changed", G_CALLBACK (thunar_standard_view_row_changed), standard_view);
  g_signal_connect (G_OBJECT (standard_view->model), "rows-reordered", G_CALLBACK (thunar_standard_view_rows_reordered), standard_view);
  g_signal_connect (G_OBJECT (standard_view->model), "error", G_CALLBACK (thunar_standard_view_error), standard_view);
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-case-sensitive", G_OBJECT (standard_view->model), "case-sensitive");
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-date-style", G_OBJECT (standard_view->model), "date-style");
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-folders-first", G_OBJECT (standard_view->model), "folders-first");
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-file-size-binary", G_OBJECT (standard_view->model), "file-size-binary");

  /* setup the icon renderer */
  standard_view->icon_renderer = thunar_icon_renderer_new ();
  g_object_ref_sink (G_OBJECT (standard_view->icon_renderer));
  exo_binding_new (G_OBJECT (standard_view), "zoom-level", G_OBJECT (standard_view->icon_renderer), "size");

  /* setup the name renderer */
  standard_view->name_renderer = thunar_text_renderer_new ();
  g_object_ref_sink (G_OBJECT (standard_view->name_renderer));
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (standard_view->name_renderer), "follow-prelit");

  /* be sure to update the selection whenever the folder changes */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::folder", G_CALLBACK (thunar_standard_view_selection_changed), standard_view);

  /* be sure to update the statusbar text whenever the number of
   * files in our model changes.
   */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::num-files", G_CALLBACK (thunar_standard_view_update_statusbar_text), standard_view);

  /* be sure to update the statusbar text whenever the file-size-binary property changes */
  g_signal_connect_swapped (G_OBJECT (standard_view->model), "notify::file-size-binary", G_CALLBACK (thunar_standard_view_update_statusbar_text), standard_view);

  /* connect to size allocation signals for generating thumbnail requests */
  g_signal_connect_after (G_OBJECT (standard_view), "size-allocate",
                          G_CALLBACK (thunar_standard_view_size_allocate), NULL);
}



static GObject*
thunar_standard_view_constructor (GType                  type,
                                  guint                  n_construct_properties,
                                  GObjectConstructParam *construct_properties)
{
  ThunarStandardView *standard_view;
  ThunarZoomLevel     zoom_level;
  GtkAdjustment      *adjustment;
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
  g_object_bind_property (object, "zoom-level", G_OBJECT (standard_view->preferences), THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->zoom_level_property_name, G_BINDING_DEFAULT);

  /* determine the real view widget (treeview or iconview) */
  view = GTK_BIN (object)->child;

  /* apply our list model to the real view (the child of the scrolled window),
   * we therefore assume that all real views have the "model" property.
   */
  g_object_set (G_OBJECT (view), "model", standard_view->model, NULL);

  /* apply the single-click settings to the view */
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-single-click", G_OBJECT (view), "single-click");
  exo_binding_new (G_OBJECT (standard_view->preferences), "misc-single-click-timeout", G_OBJECT (view), "single-click-timeout");

  /* apply the default sort column and sort order */
  g_object_get (G_OBJECT (standard_view->preferences), "last-sort-column", &sort_column, "last-sort-order", &sort_order, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (standard_view->model), sort_column, sort_order);

  /* stay informed about changes to the sort column/order */
  g_signal_connect (G_OBJECT (standard_view->model), "sort-column-changed", G_CALLBACK (thunar_standard_view_sort_column_changed), standard_view);

  /* setup support to navigate using a horizontal mouse wheel and the back and forward buttons */
  g_signal_connect (G_OBJECT (view), "scroll-event", G_CALLBACK (thunar_standard_view_scroll_event), object);
  g_signal_connect (G_OBJECT (view), "button-press-event", G_CALLBACK (thunar_standard_view_button_press_event), object);

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

  /* connect to scroll events for generating thumbnail requests */
  adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (thunar_standard_view_scrolled), object);
  adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_signal_connect (adjustment, "value-changed",
                    G_CALLBACK (thunar_standard_view_scrolled), object);

  /* done, we have a working object */
  return object;
}



static void
thunar_standard_view_dispose (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* cancel pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* unregister the "loading" binding */
  if (G_UNLIKELY (standard_view->loading_binding != NULL))
    exo_binding_unbind (standard_view->loading_binding);

  /* be sure to cancel any pending drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* be sure to cancel any pending drag timer */
  if (G_UNLIKELY (standard_view->priv->drag_timer_id != 0))
    g_source_remove (standard_view->priv->drag_timer_id);

  /* reset the UI manager property */
  thunar_component_set_ui_manager (THUNAR_COMPONENT (standard_view), NULL);

  /* disconnect from file */
  if (standard_view->priv->current_directory != NULL)
    {
      g_signal_handlers_disconnect_matched (standard_view->priv->current_directory,
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
      g_object_unref (standard_view->priv->current_directory);
      standard_view->priv->current_directory = NULL;
    }

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->dispose) (object);
}



static void
thunar_standard_view_finalize (GObject *object)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (object);

  /* some safety checks */
  _thunar_assert (standard_view->loading_binding == NULL);
  _thunar_assert (standard_view->icon_factory == NULL);
  _thunar_assert (standard_view->ui_manager == NULL);
  _thunar_assert (standard_view->clipboard == NULL);

  /* release the thumbnailer */
  g_signal_handlers_disconnect_by_func (standard_view->priv->thumbnailer, thunar_standard_view_finished_thumbnailing, standard_view);
  g_object_unref (standard_view->priv->thumbnailer);

  /* release the scroll_to_file reference (if any) */
  if (G_UNLIKELY (standard_view->priv->scroll_to_file != NULL))
    g_object_unref (G_OBJECT (standard_view->priv->scroll_to_file));

  /* release the selected_files list (if any) */
  thunar_g_file_list_free (standard_view->priv->selected_files);

  /* release our reference on the provider factory */
  g_object_unref (G_OBJECT (standard_view->priv->provider_factory));

  /* release the drag path list (just in case the drag-end wasn't fired before) */
  thunar_g_file_list_free (standard_view->priv->drag_g_file_list);

  /* release the drop path list (just in case the drag-leave wasn't fired before) */
  thunar_g_file_list_free (standard_view->priv->drop_file_list);

  /* release the history */
  g_object_unref (standard_view->priv->history);

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

  /* drop any remaining "new-files" paths */
  thunar_g_file_list_free (standard_view->priv->new_files_path_list);

  /* release our reference on the preferences */
  g_object_unref (G_OBJECT (standard_view->preferences));

  /* disconnect from the list model */
  g_signal_handlers_disconnect_matched (G_OBJECT (standard_view->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
  g_object_unref (G_OBJECT (standard_view->model));

  /* remove selection restore timeout */
  if (standard_view->priv->restore_selection_idle_id != 0)
    g_source_remove (standard_view->priv->restore_selection_idle_id);

  /* free the statusbar text (if any) */
  if (standard_view->priv->statusbar_text_idle_id != 0)
    g_source_remove (standard_view->priv->statusbar_text_idle_id);
  g_free (standard_view->priv->statusbar_text);

  /* release the scroll_to_files hash table */
  g_hash_table_destroy (standard_view->priv->scroll_to_files);

  (*G_OBJECT_CLASS (thunar_standard_view_parent_class)->finalize) (object);
}



static void
thunar_standard_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarFile *current_directory;

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_LOADING:
      g_value_set_boolean (value, thunar_view_get_loading (THUNAR_VIEW (object)));
      break;

    case PROP_DISPLAY_NAME:
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object));
      if (current_directory != NULL)
        g_value_set_static_string (value, thunar_file_get_display_name (current_directory));
      break;

    case PROP_TOOLTIP_TEXT:
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object));
      if (current_directory != NULL)
        g_value_take_string (value, g_file_get_parse_name (thunar_file_get_file (current_directory)));
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

  /* determine the icon factory for the screen on which we are realized */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
  standard_view->icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* we need to redraw whenever the "thumbnail_mode" property is toggled */
  g_signal_connect_swapped (standard_view->icon_factory,
                            "notify::thumbnail_mode",
                            G_CALLBACK (thunar_standard_view_thumbnail_mode_toggled),
                            standard_view);
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
  cairo_t *cr;
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

      /* the cairo version looks better here, so we use it if possible */
      cr = gdk_cairo_create (widget->window);
      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_set_line_width (cr, 1.0);
      cairo_rectangle (cr, x + 0.5, y + 0.5, width - 1, height - 1);
      cairo_stroke (cr);
      cairo_destroy (cr);
    }

  return result;
}



static GList*
thunar_standard_view_get_selected_files (ThunarComponent *component)
{
  return THUNAR_STANDARD_VIEW (component)->priv->selected_files;
}



static void
thunar_standard_view_set_selected_files (ThunarComponent *component,
                                         GList           *selected_files)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (component);
  GtkTreePath        *first_path = NULL;
  GList              *paths;
  GList              *lp;

  /* release the previous selected files list (if any) */
  if (G_UNLIKELY (standard_view->priv->selected_files != NULL))
    {
      thunar_g_file_list_free (standard_view->priv->selected_files);
      standard_view->priv->selected_files = NULL;
    }

  /* check if we're still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      /* remember a copy of the list for later */
      standard_view->priv->selected_files = thunar_g_file_list_copy (selected_files);
    }
  else
    {
      /* verify that we have a valid model */
      if (G_UNLIKELY (standard_view->model == NULL))
        return;

      /* unselect all previously selected files */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);

      /* determine the tree paths for the given files */
      paths = thunar_list_model_get_paths_for_files (standard_view->model, selected_files);
      if (G_LIKELY (paths != NULL))
        {
          /* determine the first path */
          for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
              /* check if this path is located before the current first_path */
              if (gtk_tree_path_compare (lp->data, first_path) < 0)
                first_path = lp->data;
            }

          /* place the cursor on the first selected path (must be first for GtkTreeView) */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor) (standard_view, first_path, FALSE);

          /* select the given tree paths paths */
          for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
              /* select the path */
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, lp->data);
            }

          /* scroll to the first path (previously determined) */
          (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->scroll_to_path) (standard_view, first_path, FALSE, 0.0f, 0.0f);

          /* release the tree paths */
          g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
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

  /* leave if nothing changed */
  if (standard_view->ui_manager == ui_manager)
    return;

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

      /* force update to remove all actions and proxies */
      gtk_ui_manager_ensure_update (standard_view->ui_manager);

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

      /* force update to avoid flickering */
      gtk_ui_manager_ensure_update (standard_view->ui_manager);
    }

  /* let others know that we have a new manager */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_UI_MANAGER]);
}



static ThunarFile*
thunar_standard_view_get_current_directory (ThunarNavigator *navigator)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);
  return standard_view->priv->current_directory;
}



static void
thunar_standard_view_scroll_position_save (ThunarStandardView *standard_view)
{
  ThunarFile    *first_file;
  GtkAdjustment *vadjustment;
  GtkAdjustment *hadjustment;
  GFile         *gfile;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* store the previous directory in the scroll hash table */
  if (standard_view->priv->current_directory != NULL)
    {
      /* only stop the first file is the scroll bar is actually moved */
      vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));
      hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));
      gfile = thunar_file_get_file (standard_view->priv->current_directory);

      if (gtk_adjustment_get_value (vadjustment) == 0.0
          && gtk_adjustment_get_value (hadjustment) == 0.0)
        {
          /* remove from the hash table, we already scroll to 0,0 */
          g_hash_table_remove (standard_view->priv->scroll_to_files, gfile);
        }
      else if (thunar_view_get_visible_range (THUNAR_VIEW (standard_view), &first_file, NULL))
        {
          /* add the file to our internal mapping of directories to scroll files */
          g_hash_table_replace (standard_view->priv->scroll_to_files,
                                g_object_ref (gfile),
                                g_object_ref (thunar_file_get_file (first_file)));
          g_object_unref (first_file);
        }
    }
}



static void
thunar_standard_view_restore_selection_from_history (ThunarStandardView *standard_view)
{
  GList       selected_files;
  ThunarFile *selected_file;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (standard_view->priv->current_directory));

  /* reset the selected files list */
  selected_files.data = NULL;
  selected_files.prev = NULL;
  selected_files.next = NULL;

  /* determine the next file in the history */
  selected_file = thunar_history_peek_forward (standard_view->priv->history);
  if (selected_file != NULL)
    {
      /* mark the file from history for selection if it is inside the new
       * directory */
      if (thunar_file_is_parent (standard_view->priv->current_directory, selected_file))
        selected_files.data = selected_file;
      else
        g_object_unref (selected_file);
    }

  /* do the same with the previous file in the history */
  if (selected_files.data == NULL)
    {
      selected_file = thunar_history_peek_back (standard_view->priv->history);
      if (selected_file != NULL)
        {
          /* mark the file from history for selection if it is inside the
           * new directory */
          if (thunar_file_is_parent (standard_view->priv->current_directory, selected_file))
            selected_files.data = selected_file;
          else
            g_object_unref (selected_file);
        }
    }

  /* select the previous or next file from the history if it is inside the
   * new current directory */
  if (selected_files.data != NULL)
    {
      thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), &selected_files);
      g_object_unref (G_OBJECT (selected_files.data));
    }
}



static void
thunar_standard_view_set_current_directory (ThunarNavigator *navigator,
                                            ThunarFile      *current_directory)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (navigator);
  ThunarFolder       *folder;
  gboolean            trashed;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* get the current directory */
  if (standard_view->priv->current_directory == current_directory)
    return;

  /* cancel any pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* disconnect any previous "loading" binding */
  if (G_LIKELY (standard_view->loading_binding != NULL))
    exo_binding_unbind (standard_view->loading_binding);

  /* store the current scroll position */
  if (current_directory != NULL)
    thunar_standard_view_scroll_position_save (standard_view);

  /* release previous directory */
  if (standard_view->priv->current_directory != NULL)
    {
      g_signal_handlers_disconnect_matched (standard_view->priv->current_directory,
                                            G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
      g_object_unref (standard_view->priv->current_directory);
    }

  /* check if we want to reset the directory */
  if (G_UNLIKELY (current_directory == NULL))
    {
      /* unset */
      standard_view->priv->current_directory = NULL;

      /* resetting the folder for the model can take some time if the view has
       * to update the selection everytime (i.e. closing a window with a lot of
       * selected files), so we temporarily disconnect the model from the view.
       */
      g_object_set (G_OBJECT (GTK_BIN (standard_view)->child), "model", NULL, NULL);

      /* reset the folder for the model */
      thunar_list_model_set_folder (standard_view->model, NULL);

      /* reconnect the model to the view */
      g_object_set (G_OBJECT (GTK_BIN (standard_view)->child), "model", standard_view->model, NULL);

      /* and we're done */
      return;
    }

  /* take ref on new directory */
  standard_view->priv->current_directory = g_object_ref (current_directory);
  g_signal_connect (G_OBJECT (current_directory), "destroy", G_CALLBACK (thunar_standard_view_current_directory_destroy), standard_view);
  g_signal_connect (G_OBJECT (current_directory), "changed", G_CALLBACK (thunar_standard_view_current_directory_changed), standard_view);

  /* scroll to top-left when changing folder */
  gtk_adjustment_set_value (gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);
  gtk_adjustment_set_value (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view)), 0.0);

  /* store the directory in the history */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (standard_view->priv->history), current_directory);

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

  /* check if the new directory is in the trash */
  trashed = thunar_file_is_trashed (current_directory);

  /* update the "Create Folder"/"Create Document" actions */
  gtk_action_set_visible (standard_view->priv->action_create_folder, !trashed);
  gtk_action_set_visible (standard_view->priv->action_create_document, !trashed);

  /* update the "Rename" action */
  gtk_action_set_visible (standard_view->priv->action_rename, !trashed);

  /* update the "Restore" action */
  gtk_action_set_visible (standard_view->priv->action_restore, trashed);

  /* schedule a thumbnail timeout */
  /* NOTE: quickly after this we always trigger a size allocate wich will handle this */

  /* notify all listeners about the new/old current directory */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_CURRENT_DIRECTORY]);

  /* update tab label and tooltip */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_DISPLAY_NAME]);
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_TOOLTIP_TEXT]);

  /* restore the selection from the history */
  thunar_standard_view_restore_selection_from_history (standard_view);
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
  GList      *new_files_path_list;
  GList      *selected_files;
  GFile      *first_file;
  ThunarFile *current_directory;

  loading = !!loading;

  /* check if we're already in that state */
  if (G_UNLIKELY (standard_view->loading == loading))
    return;

  /* apply the new state */
  standard_view->loading = loading;

  /* block or unblock the insert signal to avoid queueing thumbnail reloads */
  if (loading)
    g_signal_handler_block (standard_view->model, standard_view->priv->row_changed_id);
  else
    g_signal_handler_unblock (standard_view->model, standard_view->priv->row_changed_id);

  /* check if we're done loading and have a scheduled scroll_to_file */
  if (G_UNLIKELY (!loading))
    {
      if (standard_view->priv->scroll_to_file != NULL)
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
      else
        {
          /* look for a first visible file in the hash table */
          current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
          if (G_LIKELY (current_directory != NULL))
            {
              first_file = g_hash_table_lookup (standard_view->priv->scroll_to_files, thunar_file_get_file (current_directory));
              if (G_LIKELY (first_file != NULL))
                {
                  file = thunar_file_cache_lookup (first_file);
                  if (G_LIKELY (file != NULL))
                    {
                      thunar_view_scroll_to_file (THUNAR_VIEW (standard_view), file, FALSE, TRUE, 0.0f, 0.0f);
                      g_object_unref (file);
                    }
                }
            }
        }
    }

  /* check if we have a path list from new_files pending */
  if (G_UNLIKELY (!loading && standard_view->priv->new_files_path_list != NULL))
    {
      /* remember and reset the new_files_path_list */
      new_files_path_list = standard_view->priv->new_files_path_list;
      standard_view->priv->new_files_path_list = NULL;

      /* and try again */
      thunar_standard_view_new_files (standard_view, new_files_path_list);

      /* cleanup */
      thunar_g_file_list_free (new_files_path_list);
    }

  /* check if we're done loading */
  if (!loading)
    {
      /* remember and reset the file list */
      selected_files = standard_view->priv->selected_files;
      standard_view->priv->selected_files = NULL;

      /* and try setting the selected files again */
      thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), selected_files);

      /* cleanup */
      thunar_g_file_list_free (selected_files);
    }

  /* check if we're done loading and a thumbnail timeout or idle was requested */
  if (!loading && standard_view->priv->thumbnailing_scheduled)
    {
      /* We've just finished loading. It will probably take the user some time to
       * understand the contents of the folder before he will start interacting
       * with the view. So here we can safely schedule an idle function instead
       * of a timeout. */
      thunar_standard_view_schedule_thumbnail_idle (standard_view);
      standard_view->priv->thumbnailing_scheduled = FALSE;
    }

  /* notify listeners */
  g_object_freeze_notify (G_OBJECT (standard_view));
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_LOADING]);
  thunar_standard_view_update_statusbar_text (standard_view);
  g_object_thaw_notify (G_OBJECT (standard_view));
}



static const gchar*
thunar_standard_view_get_statusbar_text (ThunarView *view)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  GList              *items;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  /* generate the statusbar text on-demand */
  if (standard_view->priv->statusbar_text == NULL)
    {
      /* query the selected items (actually a list of GtkTreePath's) */
      items = THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items (standard_view);

      /* we display a loading text if no items are
       * selected and the view is loading
       */
      if (items == NULL && standard_view->loading)
        return _("Loading folder contents...");

      standard_view->priv->statusbar_text = thunar_list_model_get_statusbar_text (standard_view->model, items);
      g_list_free_full (items, (GDestroyNotify) gtk_tree_path_free);
    }

  return standard_view->priv->statusbar_text;
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
      g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_ZOOM_LEVEL]);
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
thunar_standard_view_reload (ThunarView *view,
                             gboolean    reload_info)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (view);
  ThunarFolder       *folder;

  /* determine the folder for the view model */
  folder = thunar_list_model_get_folder (standard_view->model);
  if (G_LIKELY (folder != NULL))
    thunar_folder_reload (folder, reload_info);

  /* schedule thumbnail reload update */
  if (!standard_view->priv->thumbnailing_scheduled)
    thunar_standard_view_schedule_thumbnail_idle (standard_view);
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
                                     gboolean    select_file,
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
      standard_view->priv->scroll_to_select = select_file;
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
          if (G_UNLIKELY (select_file))
            {
              /* select only the file in question */
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all) (standard_view);
              (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, paths->data);
            }

          /* cleanup */
          g_list_free_full (paths, (GDestroyNotify) gtk_tree_path_free);
        }
    }
}



static gboolean
thunar_standard_view_delete_selected_files (ThunarStandardView *standard_view)
{
  GtkAction       *action = GTK_ACTION (standard_view->priv->action_move_to_trash);
  const gchar     *accel_path;
  GtkAccelKey      key;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  if (thunar_g_vfs_is_uri_scheme_supported ("trash"))
    {
      /* Check if there is a user defined accelerator for the delete action,
       * if there is, skip events from the hard-coded keys which are set in
       * the class of the standard view. See bug #4173.
       *
       * The trick here is that if a custom accelerator is set by the user,
       * this function is never called. If a hardcoded key combination is
       * pressed and a custom accelerator is set, accel_key || accel_mods
       * are no 0. */
      accel_path = gtk_action_get_accel_path (action);
      if (accel_path != NULL
          && gtk_accel_map_lookup_entry (accel_path, &key)
          && (key.accel_key != 0 || key.accel_mods != 0))
        return FALSE;

      /* just emit the "activate" signal on the "move-trash" action */
      gtk_action_activate (action);
    }
  else
    {
      /* do a permanent delete */
      gtk_action_activate (GTK_ACTION (standard_view->priv->action_delete));
    }

  /* ...and we're done */
  return TRUE;
}



static GdkDragAction
thunar_standard_view_get_dest_actions (ThunarStandardView *standard_view,
                                       GdkDragContext     *context,
                                       gint                x,
                                       gint                y,
                                       guint               timestamp,
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
      actions = thunar_file_accepts_drop (file, standard_view->priv->drop_file_list, context, &action);
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
  gdk_drag_status (context, action, timestamp);

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
              files = g_list_prepend (files, file);
            }
          files = g_list_reverse (files);
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
        thunar_g_file_list_free (files);
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



static gboolean
thunar_standard_view_update_statusbar_text_idle (gpointer data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (data);

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  GDK_THREADS_ENTER ();

  /* clear the current status text (will be recalculated on-demand) */
  g_free (standard_view->priv->statusbar_text);
  standard_view->priv->statusbar_text = NULL;

  standard_view->priv->statusbar_text_idle_id = 0;

  /* tell everybody that the statusbar text may have changed */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_STATUSBAR_TEXT]);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_standard_view_update_statusbar_text (ThunarStandardView *standard_view)
{
  /* stop pending timeout */
  if (standard_view->priv->statusbar_text_idle_id != 0)
    g_source_remove (standard_view->priv->statusbar_text_idle_id);

  /* restart a new one, this way we avoid multiple update when
   * the user is pressing a key to scroll */
  standard_view->priv->statusbar_text_idle_id =
      g_timeout_add_full (G_PRIORITY_LOW, 50, thunar_standard_view_update_statusbar_text_idle,
                          standard_view, NULL);
}



/*
 * Find a fallback directory we can navigate to if the directory gets
 * deleted. It first tries the parent folders, and finally if none can
 * be found, the home folder. If the home folder cannot be accessed,
 * the error will be stored for use by the caller.
 */
static ThunarFile *
thunar_standard_view_get_fallback_directory (ThunarFile *directory,
                                             GError     *error)
{
  ThunarFile *new_directory = NULL;
  GFile      *path;
  GFile      *tmp;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (directory), NULL);

  /* determine the path of the directory */
  path = g_object_ref (thunar_file_get_file (directory));

  /* try to find a parent directory that still exists */
  while (new_directory == NULL)
    {
      /* check whether the directory exists */
      if (g_file_query_exists (path, NULL))
        {
          /* it does, try to load the file */
          new_directory = thunar_file_get (path, NULL);

          /* fall back to $HOME if loading the file failed */
          if (new_directory == NULL)
            break;
        }
      else
        {
          /* determine the parent of the directory */
          tmp = g_file_get_parent (path);

          /* if there's no parent this means that we've found no parent
           * that still exists at all. Fall back to $HOME then */
          if (tmp == NULL)
            break;

          /* free the old directory */
          g_object_unref (path);

          /* check the parent next */
          path = tmp;
        }
    }

  /* release last ref */
  if (path != NULL)
    g_object_unref (path);

  if (new_directory == NULL)
    {
      /* fall-back to the home directory */
      path = thunar_g_file_new_for_home ();
      new_directory = thunar_file_get (path, &error);
      g_object_unref (path);
    }

  return new_directory;
}



static void
thunar_standard_view_current_directory_destroy (ThunarFile         *current_directory,
                                                ThunarStandardView *standard_view)
{
  GtkWidget  *window;
  ThunarFile *new_directory = NULL;
  GError     *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (standard_view->priv->current_directory == current_directory);

  /* get a fallback directory (parents or home) we can navigate to */
  new_directory = thunar_standard_view_get_fallback_directory (current_directory, error);
  if (G_UNLIKELY (new_directory == NULL))
    {
      /* display an error to the user */
      thunar_dialogs_show_error (GTK_WIDGET (standard_view), error, _("Failed to open the home folder"));
      g_error_free (error);
      return;
    }

  /* let the parent window update all active and inactive views (tabs) */
  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  thunar_window_update_directories (THUNAR_WINDOW (window),
                                    current_directory,
                                    new_directory);

  /* release the reference to the new directory */
  g_object_unref (new_directory);
}



static void
thunar_standard_view_current_directory_changed (ThunarFile         *current_directory,
                                                ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (standard_view->priv->current_directory == current_directory);

  /* update tab label and tooltip */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_DISPLAY_NAME]);
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_TOOLTIP_TEXT]);

  /* directory is possibly moved, schedule a thumbnail update */
  thunar_standard_view_schedule_thumbnail_timeout (standard_view);
}



static void
thunar_standard_view_action_create_empty_file (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GList              path_list;
  gchar             *name;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ask the user to enter a name for the new empty file */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view),
                                    "text/plain",
                                    _("New Empty File"),
                                    _("New Empty File..."));
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the path list */
          path_list.data = g_file_resolve_relative_path (thunar_file_get_file (current_directory), name);
          path_list.next = path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_creat (application, GTK_WIDGET (standard_view), &path_list, NULL,
                                    thunar_standard_view_new_files_closure (standard_view, NULL));
          g_object_unref (application);

          /* release the path */
          g_object_unref (path_list.data);
        }

      /* release the file name in the local encoding */
      g_free (name);
    }
}



static void
thunar_standard_view_action_create_folder (GtkAction          *action,
                                           ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GList              path_list;
  gchar             *name;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view),
                                    "inode/directory",
                                    _("New Folder"),
                                    _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the path list */
          path_list.data = g_file_resolve_relative_path (thunar_file_get_file (current_directory), name);
          path_list.next = path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_mkdir (application, GTK_WIDGET (standard_view), &path_list,
                                    thunar_standard_view_new_files_closure (standard_view, NULL));
          g_object_unref (G_OBJECT (application));

          /* release the path */
          g_object_unref (path_list.data);
        }

      /* release the file name */
      g_free (name);
    }
}



static void
thunar_standard_view_action_create_template (GtkAction           *action,
                                             const ThunarFile    *file,
                                             ThunarStandardView  *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GList              target_path_list;
  gchar             *name;
  gchar             *title;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* generate a title for the create dialog */
  title = g_strdup_printf (_("Create Document from template \"%s\""),
                           thunar_file_get_display_name (file));

  /* ask the user to enter a name for the new document */
  name = thunar_show_create_dialog (GTK_WIDGET (standard_view),
                                    thunar_file_get_content_type (THUNAR_FILE (file)),
                                    thunar_file_get_display_name (file),
                                    title);
  if (G_LIKELY (name != NULL))
    {
      /* determine the ThunarFile for the current directory */
      current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
      if (G_LIKELY (current_directory != NULL))
        {
          /* fake the target path list */
          target_path_list.data = g_file_get_child (thunar_file_get_file (current_directory), name);
          target_path_list.next = NULL;
          target_path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_creat (application, GTK_WIDGET (standard_view), &target_path_list,
                                    thunar_file_get_file (file),
                                    thunar_standard_view_new_files_closure (standard_view, NULL));
          g_object_unref (G_OBJECT (application));

          /* release the target path */
          g_object_unref (target_path_list.data);
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
  ThunarFile *directory;
  GtkWidget  *toplevel;
  GtkWidget  *dialog;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* popup the files dialog */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  if (G_LIKELY (toplevel != NULL))
    {
      dialog = thunar_properties_dialog_new (GTK_WINDOW (toplevel));

      /* check if no files are currently selected */
      if (standard_view->priv->selected_files == NULL)
        {
          /* if we don't have any files selected, we just popup
           * the properties dialog for the current folder.
           */
          directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), directory);
        }
      else
        {
          /* popup the properties dialog for all file(s) */
          thunar_properties_dialog_set_files (THUNAR_PROPERTIES_DIALOG (dialog),
                                              standard_view->priv->selected_files);
        }

      gtk_widget_show (dialog);
    }
}



static void
thunar_standard_view_action_cut (GtkAction          *action,
                                 ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  thunar_clipboard_manager_cut_files (standard_view->clipboard, standard_view->priv->selected_files);
}



static void
thunar_standard_view_action_copy (GtkAction          *action,
                                  ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (standard_view->clipboard));

  thunar_clipboard_manager_copy_files (standard_view->clipboard, standard_view->priv->selected_files);
}



static void
thunar_standard_view_action_paste (GtkAction          *action,
                                   ThunarStandardView *standard_view)
{
  ThunarFile *current_directory;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      thunar_clipboard_manager_paste_files (standard_view->clipboard, thunar_file_get_file (current_directory),
                                            GTK_WIDGET (standard_view), thunar_standard_view_new_files_closure (standard_view, NULL));
    }
}



static void
thunar_standard_view_unlink_selection (ThunarStandardView *standard_view,
                                       gboolean            permanently)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* delete the selected files */
  application = thunar_application_get ();
  thunar_application_unlink_files (application, GTK_WIDGET (standard_view),
                                   standard_view->priv->selected_files,
                                   permanently);
  g_object_unref (G_OBJECT (application));

  /* do not select new files */
  thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), NULL);
}



static void
thunar_standard_view_action_move_to_trash (GtkAction          *action,
                                           ThunarStandardView *standard_view)
{
  gboolean           permanently;
  GdkModifierType    state;
  const gchar       *accel_path;
  GtkAccelKey        key;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check if we should permanently delete the files (user holds shift) */
  permanently = (gtk_get_current_event_state (&state) && (state & GDK_SHIFT_MASK) != 0);
  if (permanently)
    {
      /* look if the user has set a custom accelerator (accel_key != 0)
       * that contains a shift modifier */
      accel_path = gtk_action_get_accel_path (action);
      if (accel_path != NULL
          && gtk_accel_map_lookup_entry (accel_path, &key)
          && key.accel_key != 0
          && (key.accel_mods & GDK_SHIFT_MASK) != 0)
        permanently = FALSE;
    }

  thunar_standard_view_unlink_selection (standard_view, permanently);
}



static void
thunar_standard_view_action_delete (GtkAction          *action,
                                    ThunarStandardView *standard_view)
{
  thunar_standard_view_unlink_selection (standard_view, TRUE);
}



static void
thunar_standard_view_action_paste_into_folder (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  ThunarFile *file;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the first selected file and verify that it's a folder */
  file = g_list_nth_data (standard_view->priv->selected_files, 0);
  if (G_LIKELY (file != NULL && thunar_file_is_directory (file)))
    thunar_clipboard_manager_paste_files (standard_view->clipboard, thunar_file_get_file (file), GTK_WIDGET (standard_view), NULL);
}



static void
thunar_standard_view_action_select_all_files (GtkAction          *action,
                                              ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* grab the focus to the view */
  gtk_widget_grab_focus (GTK_WIDGET (standard_view));

  /* select all files in the real view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_all) (standard_view);
}



static void
thunar_standard_view_action_select_by_pattern (GtkAction          *action,
                                               ThunarStandardView *standard_view)
{
  GtkWidget   *window;
  GtkWidget   *dialog;
  GtkWidget   *hbox;
  GtkWidget   *label;
  GtkWidget   *entry;
  GList       *paths;
  GList       *lp;
  gint         response;
  const gchar *pattern;
  gchar       *pattern_extended = NULL;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));
  dialog = gtk_dialog_new_with_buttons (_("Select by Pattern"),
                                        GTK_WINDOW (window),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        _("_Select"), GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 290, -1);

  hbox = g_object_new (GTK_TYPE_HBOX, "border-width", 6, "spacing", 10, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new_with_mnemonic (_("_Pattern:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entry);
  gtk_widget_show (entry);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_OK)
    {
      /* get entered pattern */
      pattern = gtk_entry_get_text (GTK_ENTRY (entry));
      if (pattern != NULL
          && strchr (pattern, '*') == NULL
          && strchr (pattern, '?') == NULL)
        {
          /* make file matching pattern */
          pattern_extended = g_strdup_printf ("*%s*", pattern);
          pattern = pattern_extended;
        }

      /* select all files that match pattern */
      paths = thunar_list_model_get_paths_for_pattern (standard_view->model, pattern);
      THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->unselect_all (standard_view);

      /* set the cursor and scroll to the first selected item */
      if (paths != NULL)
        THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor (standard_view, g_list_last (paths)->data, FALSE);

      for (lp = paths; lp != NULL; lp = lp->next)
        {
          THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path (standard_view, lp->data);
          gtk_tree_path_free (lp->data);
        }
      g_list_free (paths);
      g_free (pattern_extended);
    }

  gtk_widget_destroy (dialog);
}



static void
thunar_standard_view_action_selection_invert (GtkAction          *action,
                                              ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* grab the focus to the view */
  gtk_widget_grab_focus (GTK_WIDGET (standard_view));

  /* invert all files in the real view */
  (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->selection_invert) (standard_view);
}



static void
thunar_standard_view_action_duplicate (GtkAction          *action,
                                       ThunarStandardView *standard_view)
{
  ThunarApplication *application;
  ThunarFile        *current_directory;
  GClosure          *new_files_closure;
  GList             *selected_files;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the file for the current directory */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      /* determine the selected files for the view */
      selected_files = thunar_file_list_to_thunar_g_file_list (standard_view->priv->selected_files);
      if (G_LIKELY (selected_files != NULL))
        {
          /* copy the selected files into the current directory, which effectively
           * creates duplicates of the files.
           */
          application = thunar_application_get ();
          new_files_closure = thunar_standard_view_new_files_closure (standard_view, NULL);
          thunar_application_copy_into (application, GTK_WIDGET (standard_view), selected_files,
                                        thunar_file_get_file (current_directory), new_files_closure);
          g_object_unref (G_OBJECT (application));

          /* clean up */
          thunar_g_file_list_free (selected_files);
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
  GList             *selected_files;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* determine the file for the current directory */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  if (G_LIKELY (current_directory != NULL))
    {
      /* determine the selected paths for the view */
      selected_files = thunar_file_list_to_thunar_g_file_list (standard_view->priv->selected_files);
      if (G_LIKELY (selected_files != NULL))
        {
          /* link the selected files into the current directory, which effectively
           * creates new unique links for the files.
           */
          application = thunar_application_get ();
          new_files_closure = thunar_standard_view_new_files_closure (standard_view, NULL);
          thunar_application_link_into (application, GTK_WIDGET (standard_view), selected_files,
                                        thunar_file_get_file (current_directory), new_files_closure);
          g_object_unref (G_OBJECT (application));

          /* clean up */
          thunar_g_file_list_free (selected_files);
        }
    }
}



static void
thunar_standard_view_rename_error (ExoJob             *job,
                                   GError             *error,
                                   ThunarStandardView *standard_view)
{
  GArray     *param_values;
  ThunarFile *file;

  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (error != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  param_values = thunar_simple_job_get_param_values (THUNAR_SIMPLE_JOB (job));
  file = g_value_get_object (&g_array_index (param_values, GValue, 0));

  /* display an error message */
  thunar_dialogs_show_error (GTK_WIDGET (standard_view), error,
                             _("Failed to rename \"%s\""),
                             thunar_file_get_display_name (file));
}



static void
thunar_standard_view_rename_finished (ExoJob             *job,
                                      ThunarStandardView *standard_view)
{
  GArray     *param_values;
  ThunarFile *file;

  _thunar_return_if_fail (EXO_IS_JOB (job));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  param_values = thunar_simple_job_get_param_values (THUNAR_SIMPLE_JOB (job));
  file = g_value_get_object (&g_array_index (param_values, GValue, 0));

  /* make sure the file is still visible */
  thunar_view_scroll_to_file (THUNAR_VIEW (standard_view), file, TRUE, FALSE, 0.0f, 0.0f);

  /* update the selection, so we get updated actions, statusbar,
   * etc. with the new file name and probably new mime type.
   */
  thunar_standard_view_selection_changed (standard_view);

  /* destroy the job */
  g_signal_handlers_disconnect_matched (job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
  g_object_unref (job);
}



static void
thunar_standard_view_action_rename (GtkAction          *action,
                                    ThunarStandardView *standard_view)
{
  ThunarFile      *file;
  GtkWidget       *window;
  ThunarJob       *job;
  GdkModifierType  state;
  gboolean         force_bulk_renamer;
  const gchar     *accel_path;
  GtkAccelKey      key;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* leave if no files are selected */
  if (G_UNLIKELY (standard_view->priv->selected_files == NULL))
    return;

  /* open the bulk renamer also with one file selected and shift */
  force_bulk_renamer = (gtk_get_current_event_state (&state) && (state & GDK_SHIFT_MASK) != 0);
  if (G_UNLIKELY (force_bulk_renamer))
    {
      /* Check if the user defined a custom accelerator that includes the
       * shift button. If he or she has, we won't force the bulk renamer. */
      accel_path = gtk_action_get_accel_path (action);
      if (accel_path != NULL
          && gtk_accel_map_lookup_entry (accel_path, &key)
          && (key.accel_mods & GDK_SHIFT_MASK) != 0)
        force_bulk_renamer = FALSE;
    }

  /* start renaming if we have exactly one selected file */
  if (!force_bulk_renamer
      && standard_view->priv->selected_files->next == NULL)
    {
      /* get the window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (standard_view));

      /* get the file */
      file = THUNAR_FILE (standard_view->priv->selected_files->data);

      /* run the rename dialog */
      job = thunar_dialogs_show_rename_file (GTK_WINDOW (window), file);
      if (G_LIKELY (job != NULL))
        {
          g_signal_connect (job, "error", G_CALLBACK (thunar_standard_view_rename_error), standard_view);
          g_signal_connect (job, "finished", G_CALLBACK (thunar_standard_view_rename_finished), standard_view);
        }
    }
  else
    {
      /* determine the current directory of the view */
      file = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));

      /* display the bulk rename dialog */
      thunar_show_renamer_dialog (GTK_WIDGET (standard_view), file, standard_view->priv->selected_files, FALSE, NULL);
    }
}



static void
thunar_standard_view_action_restore (GtkAction          *action,
                                     ThunarStandardView *standard_view)
{
  ThunarApplication *application;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* restore the selected files */
  application = thunar_application_get ();
  thunar_application_restore_files (application, GTK_WIDGET (standard_view), standard_view->priv->selected_files,
                                    thunar_standard_view_new_files_closure (standard_view, NULL));
  g_object_unref (G_OBJECT (application));
}



static GClosure*
thunar_standard_view_new_files_closure (ThunarStandardView *standard_view,
                                        GtkWidget          *source_view)
{
  _thunar_return_val_if_fail (source_view == NULL || THUNAR_IS_VIEW (source_view), NULL);

  /* drop any previous "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
    }

  /* set the remove view data we possibly need to reload */
  g_object_set_data (G_OBJECT (standard_view), I_("source-view"), source_view);

  /* allocate a new "new-files" closure */
  standard_view->priv->new_files_closure = g_cclosure_new_swap (G_CALLBACK (thunar_standard_view_new_files), standard_view, NULL);
  g_closure_ref (standard_view->priv->new_files_closure);
  g_closure_sink (standard_view->priv->new_files_closure);

  /* and return our new closure */
  return standard_view->priv->new_files_closure;
}



static void
thunar_standard_view_new_files (ThunarStandardView *standard_view,
                                GList              *path_list)
{
  ThunarFile*file;
  GList     *file_list = NULL;
  GList     *lp;
  GtkWidget *source_view;
  GFile     *parent_file;
  gboolean   belongs_here;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* release the previous "new-files" paths (if any) */
  if (G_UNLIKELY (standard_view->priv->new_files_path_list != NULL))
    {
      thunar_g_file_list_free (standard_view->priv->new_files_path_list);
      standard_view->priv->new_files_path_list = NULL;
    }

  /* check if the folder is currently being loaded */
  if (G_UNLIKELY (standard_view->loading))
    {
      /* schedule the "new-files" paths for later processing */
      standard_view->priv->new_files_path_list = thunar_g_file_list_copy (path_list);
    }
  else if (G_LIKELY (path_list != NULL))
    {
      /* to check if we should reload */
      parent_file = thunar_file_get_file (standard_view->priv->current_directory);
      belongs_here = FALSE;

      /* determine the files for the paths */
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          file = thunar_file_cache_lookup (lp->data);
          if (G_LIKELY (file != NULL))
            file_list = g_list_prepend (file_list, file);
          else if (!belongs_here && g_file_has_parent (lp->data, parent_file))
            belongs_here = TRUE;
        }

      /* check if we have any new files here */
      if (G_LIKELY (file_list != NULL))
        {
          /* select the files */
          thunar_component_set_selected_files (THUNAR_COMPONENT (standard_view), file_list);

          /* release the file list */
          g_list_free_full (file_list, g_object_unref);

          /* grab the focus to the view widget */
          gtk_widget_grab_focus (GTK_BIN (standard_view)->child);
        }
      else if (belongs_here)
        {
          /* thunar files are not created yet, try again later because we know
           * some of them belong in this directory, so eventually they
           * will get a ThunarFile */
          standard_view->priv->new_files_path_list = thunar_g_file_list_copy (path_list);
        }
    }

  /* when performing dnd between 2 views, we force a reload on the source as well */
  source_view = g_object_get_data (G_OBJECT (standard_view), I_("source-view"));
  if (THUNAR_IS_VIEW (source_view))
    thunar_view_reload (THUNAR_VIEW (source_view), FALSE);
}



static gboolean
thunar_standard_view_button_release_event (GtkWidget          *view,
                                           GdkEventButton     *event,
                                           ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (standard_view->priv->drag_timer_id != 0, FALSE);

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

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (standard_view->priv->drag_timer_id != 0, FALSE);

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
  GdkEventButton fake_event;
  gboolean       misc_horizontal_wheel_navigates;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  if (G_UNLIKELY (event->direction == GDK_SCROLL_LEFT || event->direction == GDK_SCROLL_RIGHT))
    {
      /* check if we should use the horizontal mouse wheel for navigation */
      g_object_get (G_OBJECT (standard_view->preferences), "misc-horizontal-wheel-navigates", &misc_horizontal_wheel_navigates, NULL);
      if (G_UNLIKELY (misc_horizontal_wheel_navigates))
        {
          /* create a fake event (8 == back, 9 forward) */
          fake_event.type = GDK_BUTTON_PRESS;
          fake_event.button = event->direction == GDK_SCROLL_LEFT ? 8 : 9;

          /* trigger a fake button press event */
          return thunar_standard_view_button_press_event (view, &fake_event, standard_view);
        }
    }

  /* zoom-in/zoom-out on control+mouse wheel */
  if ((event->state & GDK_CONTROL_MASK) != 0 && (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN))
    {
      thunar_view_set_zoom_level (THUNAR_VIEW (standard_view),
          (event->direction == GDK_SCROLL_UP)
          ? MIN (standard_view->priv->zoom_level + 1, THUNAR_ZOOM_N_LEVELS - 1)
          : MAX (standard_view->priv->zoom_level, 1) - 1);
      return TRUE;
    }

  /* next please... */
  return FALSE;
}



static gboolean
thunar_standard_view_button_press_event (GtkWidget          *view,
                                         GdkEventButton     *event,
                                         ThunarStandardView *standard_view)
{
  GtkAction *action = NULL;

  if (G_LIKELY (event->type == GDK_BUTTON_PRESS))
    {
      /* determine the appropriate action ("back" for button 8, "forward" for button 9) */
      if (G_UNLIKELY (event->button == 8))
        action = gtk_ui_manager_get_action (standard_view->ui_manager, "/main-menu/go-menu/placeholder-go-history-actions/back");
      else if (G_UNLIKELY (event->button == 9))
        action = gtk_ui_manager_get_action (standard_view->ui_manager, "/main-menu/go-menu/placeholder-go-history-actions/forward");

      /* perform the action (if any) */
      if (G_UNLIKELY (action != NULL))
        {
          gtk_action_activate (action);
          return TRUE;
        }
    }

  /* next please... */
  return FALSE;
}



static gboolean
thunar_standard_view_key_press_event (GtkWidget          *view,
                                      GdkEventKey        *event,
                                      ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* need to catch "/" and "~" first, as the views would otherwise start interactive search */
  if ((event->keyval == GDK_slash || event->keyval == GDK_asciitilde || event->keyval == GDK_dead_tilde) && !(event->state & (~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask ())))
    {
      /* popup the location selector (in whatever way) */
      if (event->keyval == GDK_dead_tilde)
        g_signal_emit (G_OBJECT (standard_view), standard_view_signals[START_OPEN_LOCATION], 0, "~");
      else
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
                                guint               timestamp,
                                ThunarStandardView *standard_view)
{
  ThunarFile *file = NULL;
  GdkAtom     target;
  guchar     *prop_text;
  GFile      *path;
  gchar      *uri = NULL;
  gint        prop_len;

  target = gtk_drag_dest_find_target (view, context, NULL);
  if (G_UNLIKELY (target == GDK_NONE))
    {
      /* we cannot handle the drag data */
      return FALSE;
    }
  else if (G_UNLIKELY (target == gdk_atom_intern_static_string ("XdndDirectSave0")))
    {
      /* determine the file for the drop position */
      file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
      if (G_LIKELY (file != NULL))
        {
          /* determine the file name from the DnD source window */
          if (gdk_property_get (context->source_window, gdk_atom_intern_static_string ("XdndDirectSave0"),
                                gdk_atom_intern_static_string ("text/plain"), 0, 1024, FALSE, NULL, NULL,
                                &prop_len, &prop_text) && prop_text != NULL)
            {
              /* zero-terminate the string */
              prop_text = g_realloc (prop_text, prop_len + 1);
              prop_text[prop_len] = '\0';

              /* verify that the file name provided by the source is valid */
              if (G_LIKELY (*prop_text != '\0' && strchr ((const gchar *) prop_text, G_DIR_SEPARATOR) == NULL))
                {
                  /* allocate the relative path for the target */
                  path = g_file_resolve_relative_path (thunar_file_get_file (file),
                                                       (const gchar *)prop_text);

                  /* determine the new URI */
                  uri = g_file_get_uri (path);

                  /* setup the property */
                  gdk_property_change (GDK_DRAWABLE (context->source_window),
                                       gdk_atom_intern_static_string ("XdndDirectSave0"),
                                       gdk_atom_intern_static_string ("text/plain"), 8,
                                       GDK_PROP_MODE_REPLACE, (const guchar *) uri,
                                       strlen (uri));

                  /* cleanup */
                  g_object_unref (path);
                  g_free (uri);
                }
              else
                {
                  /* tell the user that the file name provided by the X Direct Save source is invalid */
                  thunar_dialogs_show_error (GTK_WIDGET (standard_view), NULL, _("Invalid filename provided by XDS drag site"));
                }

              /* cleanup */
              g_free (prop_text);
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
  gtk_drag_get_data (view, context, target, timestamp);

  /* we'll call gtk_drag_finish() later */
  return TRUE;
}



static void
tsv_reload_directory (GPid     pid,
                      gint     status,
                      gpointer user_data)
{
  GFileMonitor *monitor;
  GFile        *file;

  /* determine the path for the directory */
  file = g_file_new_for_uri (user_data);

  /* schedule a changed event for the directory */
  monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, NULL);
  if (monitor != NULL)
    {
      g_file_monitor_emit_event (monitor, file, NULL, G_FILE_MONITOR_EVENT_CHANGED);
      g_object_unref (monitor);
    }

  g_object_unref (file);
}



static void
thunar_standard_view_drag_data_received (GtkWidget          *view,
                                         GdkDragContext     *context,
                                         gint                x,
                                         gint                y,
                                         GtkSelectionData   *selection_data,
                                         guint               info,
                                         guint               timestamp,
                                         ThunarStandardView *standard_view)
{
  GdkDragAction actions;
  GdkDragAction action;
  ThunarFolder *folder;
  ThunarFile   *file = NULL;
  GtkWidget    *toplevel;
  gboolean      succeed = FALSE;
  GError       *error = NULL;
  gchar        *working_directory;
  gchar        *argv[11];
  gchar       **bits;
  gint          pid;
  gint          n = 0;
  GtkWidget    *source_widget;
  GtkWidget    *source_view = NULL;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!standard_view->priv->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (info == TARGET_TEXT_URI_LIST && selection_data->format == 8 && selection_data->length > 0)
        standard_view->priv->drop_file_list = thunar_g_file_list_new_from_string ((gchar *) selection_data->data);

      /* reset the state */
      standard_view->priv->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (standard_view->priv->drop_occurred))
    {
      /* reset the state */
      standard_view->priv->drop_occurred = FALSE;

      /* check if we're doing XdndDirectSave */
      if (G_UNLIKELY (info == TARGET_XDND_DIRECT_SAVE0))
        {
          /* we don't handle XdndDirectSave stage (3), result "F" yet */
          if (G_UNLIKELY (selection_data->format == 8 && selection_data->length == 1 && selection_data->data[0] == 'F'))
            {
              /* indicate that we don't provide "F" fallback */
              gdk_property_change (GDK_DRAWABLE (context->source_window),
                                   gdk_atom_intern_static_string ("XdndDirectSave0"),
                                   gdk_atom_intern_static_string ("text/plain"), 8,
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
                      thunar_folder_reload (folder, FALSE);
                      g_object_unref (G_OBJECT (folder));
                    }

                  /* cleanup */
                  g_object_unref (G_OBJECT (file));
                }
            }

          /* in either case, we succeed! */
          succeed = TRUE;
        }
      else if (G_UNLIKELY (info == TARGET_NETSCAPE_URL))
        {
          /* check if the format is valid and we have any data */
          if (G_LIKELY (selection_data->format == 8 && selection_data->length > 0))
            {
              /* _NETSCAPE_URL looks like this: "$URL\n$TITLE" */
              bits = g_strsplit ((const gchar *) selection_data->data, "\n", -1);
              if (G_LIKELY (g_strv_length (bits) == 2))
                {
                  /* determine the file for the drop position */
                  file = thunar_standard_view_get_drop_file (standard_view, x, y, NULL);
                  if (G_LIKELY (file != NULL))
                    {
                      /* determine the absolute path to the target directory */
                      working_directory = thunar_file_dup_uri (file);

                      /* prepare the basic part of the command */
                      argv[n++] = "exo-desktop-item-edit";
                      argv[n++] = "--type=Link";
                      argv[n++] = "--url";
                      argv[n++] = bits[0];
                      argv[n++] = "--name";
                      argv[n++] = bits[1];

                      /* determine the toplevel window */
                      toplevel = gtk_widget_get_toplevel (view);
                      if (toplevel != NULL && gtk_widget_is_toplevel (toplevel))
                        {
#if defined(GDK_WINDOWING_X11)
                          /* on X11, we can supply the parent window id here */
                          argv[n++] = "--xid";
                          argv[n++] = g_newa (gchar, 32);
                          g_snprintf (argv[n - 1], 32, "%ld", (glong) GDK_WINDOW_XID (toplevel->window));
#endif
                        }

                      /* terminate the parameter list */
                      argv[n++] = "--create-new";
                      argv[n++] = working_directory;
                      argv[n++] = NULL;

                      /* try to run exo-desktop-item-edit */
                      succeed = gdk_spawn_on_screen (gtk_widget_get_screen (view), working_directory, argv, NULL,
                                                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                                     NULL, NULL, &pid, &error);
                      if (G_UNLIKELY (!succeed))
                        {
                          /* display an error dialog to the user */
                          thunar_dialogs_show_error (standard_view, error, _("Failed to create a link for the URL \"%s\""), bits[0]);
                          g_free (working_directory);
                          g_error_free (error);
                        }
                      else
                        {
                          /* reload the directory when the command terminates */
                          g_child_watch_add_full (G_PRIORITY_LOW, pid, tsv_reload_directory, working_directory, g_free);
                        }

                      /* cleanup */
                      g_object_unref (G_OBJECT (file));
                    }
                }

              /* cleanup */
              g_strfreev (bits);
            }
        }
      else if (G_LIKELY (info == TARGET_TEXT_URI_LIST))
        {
          /* determine the drop position */
          actions = thunar_standard_view_get_dest_actions (standard_view, context, x, y, timestamp, &file);
          if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
            {
              /* ask the user what to do with the drop data */
              action = (context->action == GDK_ACTION_ASK)
                     ? thunar_dnd_ask (GTK_WIDGET (standard_view), file, standard_view->priv->drop_file_list, timestamp, actions)
                     : context->action;

              /* perform the requested action */
              if (G_LIKELY (action != 0))
                {
                  /* look if we can find the drag source widget */
                  source_widget = gtk_drag_get_source_widget (context);
                  if (source_widget != NULL)
                    {
                      /* if this is a source view, attach it to the view receiving
                       * the data, see thunar_standard_view_new_files */
                      source_view = gtk_widget_get_parent (source_widget);
                      if (!THUNAR_IS_VIEW (source_view))
                        source_view = NULL;
                    }

                  succeed = thunar_dnd_perform (GTK_WIDGET (standard_view), file, standard_view->priv->drop_file_list,
                                                action, thunar_standard_view_new_files_closure (standard_view, source_view));
                }
            }

          /* release the file reference */
          if (G_LIKELY (file != NULL))
            g_object_unref (G_OBJECT (file));
        }

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, timestamp);

      /* disable the highlighting and release the drag data */
      thunar_standard_view_drag_leave (view, context, timestamp, standard_view);
    }
}



static void
thunar_standard_view_drag_leave (GtkWidget          *widget,
                                 GdkDragContext     *context,
                                 guint               timestamp,
                                 ThunarStandardView *standard_view)
{
  /* reset the drop-file for the icon renderer */
  g_object_set (G_OBJECT (standard_view->icon_renderer), "drop-file", NULL, NULL);

  /* stop any running drag autoscroll timer */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
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
      thunar_g_file_list_free (standard_view->priv->drop_file_list);
      standard_view->priv->drop_file_list = NULL;
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
                                  guint               timestamp,
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

      if ((target == gdk_atom_intern_static_string ("XdndDirectSave0")) || (target == gdk_atom_intern_static_string ("_NETSCAPE_URL")))
        {
          /* determine the file for the given coordinates */
          file = thunar_standard_view_get_drop_file (standard_view, x, y, &path);

          /* check if we can save here */
          if (G_LIKELY (file != NULL
                        && thunar_file_is_local (file)
                        && thunar_file_is_directory (file)
                        && thunar_file_is_writable (file)))
            {
              action = context->suggested_action;
            }

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
          if (target != GDK_NONE)
            gtk_drag_get_data (view, context, target, timestamp);
        }

      /* tell Gdk whether we can drop here */
      gdk_drag_status (context, action, timestamp);
    }
  else
    {
      /* check whether we can drop at (x,y) */
      thunar_standard_view_get_dest_actions (standard_view, context, x, y, timestamp, NULL);
    }

  /* start the drag autoscroll timer if not already running */
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id == 0))
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
  thunar_g_file_list_free (standard_view->priv->drag_g_file_list);

  /* query the list of selected URIs */
  standard_view->priv->drag_g_file_list = thunar_file_list_to_thunar_g_file_list (standard_view->priv->selected_files);
  if (G_LIKELY (standard_view->priv->drag_g_file_list != NULL))
    {
      /* determine the first selected file */
      file = thunar_file_get (standard_view->priv->drag_g_file_list->data, NULL);
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
                                    guint               timestamp,
                                    ThunarStandardView *standard_view)
{
  gchar **uris;

  /* set the URI list for the drag selection */
  if (standard_view->priv->drag_g_file_list != NULL)
    {
      uris = thunar_g_file_list_to_stringv (standard_view->priv->drag_g_file_list);
      gtk_selection_data_set_uris (selection_data, uris);
      g_strfreev (uris);
    }
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
  if (G_UNLIKELY (standard_view->priv->drag_scroll_timer_id != 0))
    g_source_remove (standard_view->priv->drag_scroll_timer_id);

  /* release the list of dragged URIs */
  thunar_g_file_list_free (standard_view->priv->drag_g_file_list);
  standard_view->priv->drag_g_file_list = NULL;
}



static void
thunar_standard_view_row_deleted (ThunarListModel    *model,
                                  GtkTreePath        *path,
                                  ThunarStandardView *standard_view)
{
  GtkTreePath *path_copy;
  GList       *selected_items;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (path != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* Get tree paths of selected files */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);

  /* Do nothing if the deleted row is not selected or there is more than one file selected */
  if (G_UNLIKELY (g_list_find_custom (selected_items, path, (GCompareFunc) gtk_tree_path_compare) == NULL || g_list_length (selected_items) != 1))
    {
      g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);
      return;
    }

  /* Create a copy the path (we're not allowed to modify it in this handler) */
  path_copy = gtk_tree_path_copy (path);

  /* Remember the selected path so that it can be restored after the row has
   * been removed. If the first row is removed, select the first row after the
   * removal, if any other row is removed, select the row before that one */
  if (G_LIKELY (gtk_tree_path_prev (path_copy)))
    {
      standard_view->priv->selection_before_delete = path_copy;
    }
  else
    {
      standard_view->priv->selection_before_delete = gtk_tree_path_new_first ();
      gtk_tree_path_free (path_copy);
    }

  /* Free path list */
  g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);
}



static gboolean
thunar_standard_view_restore_selection_idle (ThunarStandardView *standard_view)
{
  GtkAdjustment *hadjustment;
  GtkAdjustment *vadjustment;
  gdouble        h, v, hl, hu, vl, vu;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);

  /* save the current scroll position and limits */
  hadjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));
  vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));
  g_object_get (G_OBJECT (hadjustment), "value", &h, "lower", &hl, "upper", &hu, NULL);
  g_object_get (G_OBJECT (vadjustment), "value", &v, "lower", &vl, "upper", &vu, NULL);

  /* keep the current scroll position by setting the limits to the current value */
  g_object_set (G_OBJECT (hadjustment), "lower", h, "upper", h, NULL);
  g_object_set (G_OBJECT (vadjustment), "lower", v, "upper", v, NULL);

  /* restore the selection */
  thunar_component_restore_selection (THUNAR_COMPONENT (standard_view));
  standard_view->priv->restore_selection_idle_id = 0;

  /* unfreeze the scroll position */
  g_object_set (G_OBJECT (hadjustment), "value", h, "lower", hl, "upper", hu, NULL);
  g_object_set (G_OBJECT (vadjustment), "value", v, "lower", vl, "upper", vu, NULL);

  return FALSE;
}



static void
thunar_standard_view_rows_reordered (ThunarListModel    *model,
                                     GtkTreePath        *path,
                                     GtkTreeIter        *iter,
                                     gpointer            new_order,
                                     ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* the order of the paths might have changed, but the selection
   * stayed the same, so restore the selection of the proper files
   * after letting row changes accumulate a bit */
  if (standard_view->priv->restore_selection_idle_id == 0)
    standard_view->priv->restore_selection_idle_id =
      g_timeout_add (50,
                     (GSourceFunc) thunar_standard_view_restore_selection_idle,
                     standard_view);
}



static void
thunar_standard_view_row_changed (ThunarListModel    *model,
                                  GtkTreePath        *path,
                                  GtkTreeIter        *iter,
                                  ThunarStandardView *standard_view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (path != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  if (standard_view->priv->thumbnail_request != 0)
    return;

  /* leave if this view is not suitable for generating thumbnails */
  if (!thunar_icon_factory_get_show_thumbnail (standard_view->icon_factory,
                                               standard_view->priv->current_directory))
    return;

  /* queue a thumbnail request */
  file = thunar_list_model_get_file (standard_view->model, iter);
  if (thunar_file_get_thumb_state (file) == THUNAR_FILE_THUMB_STATE_UNKNOWN)
    {
      thunar_standard_view_cancel_thumbnailing (standard_view);
      thunar_thumbnailer_queue_file (standard_view->priv->thumbnailer, file,
                                     &standard_view->priv->thumbnail_request);
    }
  g_object_unref (G_OBJECT (file));
}



static void
thunar_standard_view_select_after_row_deleted (ThunarListModel    *model,
                                               GtkTreePath        *path,
                                               ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (path != NULL);
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

  /* Check if there was only one file selected before the row was deleted. The
   * path is set by thunar_standard_view_row_deleted() if this is the case */
  if (G_LIKELY (standard_view->priv->selection_before_delete != NULL))
    {
      /* Restore the selection by selecting either the row before or the new first row */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->select_path) (standard_view, standard_view->priv->selection_before_delete);

      /* place the cursor on the selected path */
      (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->set_cursor) (standard_view, standard_view->priv->selection_before_delete, FALSE);

      /* Free the tree path */
      gtk_tree_path_free (standard_view->priv->selection_before_delete);
      standard_view->priv->selection_before_delete = NULL;
    }
}



static void
thunar_standard_view_error (ThunarListModel    *model,
                            const GError       *error,
                            ThunarStandardView *standard_view)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_LIST_MODEL (model));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (standard_view->model == model);

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
  GtkSortType      sort_order;
  gint             sort_column;

  _thunar_return_if_fail (GTK_IS_TREE_SORTABLE (tree_sortable));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* keep the currently selected files selected after the change */
  thunar_component_restore_selection (THUNAR_COMPONENT (standard_view));

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
      g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_LOADING]);
      thunar_standard_view_update_statusbar_text (standard_view);
      g_object_thaw_notify (G_OBJECT (standard_view));
    }
}



static gboolean
thunar_standard_view_drag_scroll_timer (gpointer user_data)
{
  ThunarStandardView *standard_view = THUNAR_STANDARD_VIEW (user_data);
  GtkAdjustment      *adjustment;
  gfloat              value;
  gint                offset;
  gint                y, x;
  gint                w, h;

  GDK_THREADS_ENTER ();

  /* verify that we are realized */
  if (G_LIKELY (gtk_widget_get_realized (GTK_WIDGET (standard_view))))
    {
      /* determine pointer location and window geometry */
      gdk_window_get_pointer (GTK_BIN (standard_view)->child->window, &x, &y, NULL);
      gdk_window_get_geometry (GTK_BIN (standard_view)->child->window, NULL, NULL, &w, &h, NULL);

      /* check if we are near the edge (vertical) */
      offset = y - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (y - (h - 2 * 20), 0);

      /* change the vertical adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));

          /* determine the new value */
          value = CLAMP (adjustment->value + 2 * offset, adjustment->lower, adjustment->upper - adjustment->page_size);

          /* apply the new value */
          gtk_adjustment_set_value (adjustment, value);
        }

      /* check if we are near the edge (horizontal) */
      offset = x - (2 * 20);
      if (G_UNLIKELY (offset > 0))
        offset = MAX (x - (w - 2 * 20), 0);

      /* change the horizontal adjustment appropriately */
      if (G_UNLIKELY (offset != 0))
        {
          /* determine the vertical adjustment */
          adjustment = gtk_scrolled_window_get_hadjustment (GTK_SCROLLED_WINDOW (standard_view));

          /* determine the new value */
          value = CLAMP (adjustment->value + 2 * offset, adjustment->lower, adjustment->upper - adjustment->page_size);

          /* apply the new value */
          gtk_adjustment_set_value (adjustment, value);
        }
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_standard_view_drag_scroll_timer_destroy (gpointer user_data)
{
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_scroll_timer_id = 0;
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
  THUNAR_STANDARD_VIEW (user_data)->priv->drag_timer_id = 0;
}



static void
thunar_standard_view_finished_thumbnailing (ThunarThumbnailer  *thumbnailer,
                                            guint               request,
                                            ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  if (standard_view->priv->thumbnail_request == request)
    standard_view->priv->thumbnail_request = 0;
}



static void
thunar_standard_view_thumbnailing_destroyed (gpointer data)
{
  ThunarStandardView *standard_view = data;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  standard_view->priv->thumbnail_source_id = 0;
}



static void
thunar_standard_view_cancel_thumbnailing (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* check if we have a pending thumbnail timeout/idle handler */
  if (standard_view->priv->thumbnail_source_id > 0)
    g_source_remove (standard_view->priv->thumbnail_source_id);

  /* check if we have a pending thumbnail request */
  if (standard_view->priv->thumbnail_request > 0)
    {
      /* cancel the request */
      thunar_thumbnailer_dequeue (standard_view->priv->thumbnailer,
                                  standard_view->priv->thumbnail_request);
      standard_view->priv->thumbnail_request = 0;
    }
}



static void
thunar_standard_view_schedule_thumbnail_timeout (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* delay creating the idle until the view has finished loading.
   * this is done because we only can tell the visible range reliably after
   * all items have been added and we've perhaps scrolled to the file remember
   * the last time */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      standard_view->priv->thumbnailing_scheduled = TRUE;
      return;
    }

  /* cancel any pending thumbnail sources and requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* schedule the timeout handler */
  g_assert (standard_view->priv->thumbnail_source_id == 0);
  standard_view->priv->thumbnail_source_id =
    g_timeout_add_full (G_PRIORITY_DEFAULT, 175, thunar_standard_view_request_thumbnails_lazy,
                        standard_view, thunar_standard_view_thumbnailing_destroyed);
}



static void
thunar_standard_view_schedule_thumbnail_idle (ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* delay creating the idle until the view has finished loading.
   * this is done because we only can tell the visible range reliably after
   * all items have been added, layouting has finished and we've perhaps
   * scrolled to the file remembered the last time */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    {
      standard_view->priv->thumbnailing_scheduled = TRUE;
      return;
    }

  /* cancel any pending thumbnail sources or requests */
  thunar_standard_view_cancel_thumbnailing (standard_view);

  /* schedule the timeout or idle handler */
  g_assert (standard_view->priv->thumbnail_source_id == 0);
  standard_view->priv->thumbnail_source_id =
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, thunar_standard_view_request_thumbnails,
                     standard_view, thunar_standard_view_thumbnailing_destroyed);
}



static gboolean
thunar_standard_view_request_thumbnails_real (ThunarStandardView *standard_view,
                                              gboolean            lazy_request)
{
  GtkTreePath *start_path;
  GtkTreePath *end_path;
  GtkTreePath *path;
  GtkTreeIter  iter;
  ThunarFile  *file;
  gboolean     valid_iter;
  GList       *visible_files = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_ICON_FACTORY (standard_view->icon_factory), FALSE);

  /* do nothing if we are not supposed to show thumbnails at all */
  if (!thunar_icon_factory_get_show_thumbnail (standard_view->icon_factory,
                                               standard_view->priv->current_directory))
    return FALSE;

  /* reschedule the source if we're still loading the folder */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return TRUE;

  /* compute visible item range */
  if ((*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_visible_range) (standard_view,
                                                                            &start_path,
                                                                            &end_path))
    {
      /* iterate over the range to collect all files */
      valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model),
                                            &iter, start_path);

      while (valid_iter)
        {
          /* prepend the file to the visible items list */
          file = thunar_list_model_get_file (standard_view->model, &iter);
          visible_files = g_list_prepend (visible_files, file);

          /* check if we've reached the end of the visible range */
          path = gtk_tree_model_get_path (GTK_TREE_MODEL (standard_view->model), &iter);
          if (gtk_tree_path_compare (path, end_path) != 0)
            {
              /* try to compute the next visible item */
              valid_iter =
                gtk_tree_model_iter_next (GTK_TREE_MODEL (standard_view->model), &iter);
            }
          else
            {
              /* we have reached the end, terminate the loop */
              valid_iter = FALSE;
            }

          /* release the tree path */
          gtk_tree_path_free (path);
        }

      /* queue a thumbnail request */
      thunar_thumbnailer_queue_files (standard_view->priv->thumbnailer,
                                      lazy_request, visible_files,
                                      &standard_view->priv->thumbnail_request);

      /* release the file list */
      g_list_free_full (visible_files, g_object_unref);

      /* release the start and end path */
      gtk_tree_path_free (start_path);
      gtk_tree_path_free (end_path);
    }

  return FALSE;
}



static gboolean
thunar_standard_view_request_thumbnails (gpointer data)
{
  return thunar_standard_view_request_thumbnails_real (data, FALSE);
}



static gboolean
thunar_standard_view_request_thumbnails_lazy (gpointer data)
{
  return thunar_standard_view_request_thumbnails_real (data, TRUE);
}



static void
thunar_standard_view_thumbnail_mode_toggled (ThunarStandardView *standard_view,
                                             GParamSpec         *pspec,
                                             ThunarIconFactory  *icon_factory)
{
  GtkAdjustment *vadjustment;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (THUNAR_IS_ICON_FACTORY (icon_factory));
  _thunar_return_if_fail (standard_view->icon_factory == icon_factory);

  /* check whether the user wants us to generate thumbnails */
  if (thunar_icon_factory_get_show_thumbnail (icon_factory,
                                              standard_view->priv->current_directory))
    {
      /* get the vertical adjustment of the view */
      vadjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (standard_view));

      /* fake a scroll event to generate thumbnail requests */
      thunar_standard_view_scrolled (vadjustment, standard_view);
    }
  else
    {
      /* cancel any pending thumbnail requests */
      thunar_standard_view_cancel_thumbnailing (standard_view);
    }
}



static void
thunar_standard_view_scrolled (GtkAdjustment      *adjustment,
                               ThunarStandardView *standard_view)
{
  _thunar_return_if_fail (GTK_IS_ADJUSTMENT (adjustment));
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore adjustment changes when the view is still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return;

  /* reschedule a thumbnail request timeout */
  thunar_standard_view_schedule_thumbnail_timeout (standard_view);
}



static void
thunar_standard_view_size_allocate (ThunarStandardView *standard_view,
                                    GtkAllocation      *allocation)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* ignore size changes when the view is still loading */
  if (thunar_view_get_loading (THUNAR_VIEW (standard_view)))
    return;

  /* to avoid a flow of updates, don't update if there is already a request pending */
  if (standard_view->priv->thumbnail_source_id == 0)
    {
      /* reschedule a thumbnail request timeout */
      thunar_standard_view_schedule_thumbnail_timeout (standard_view);
    }
}



/**
 * thunar_standard_view_context_menu:
 * @standard_view : a #ThunarStandardView instance.
 * @button        : the mouse button which triggered the context menu or %0 if
 *                  the event wasn't triggered by a pointer device.
 * @timestamp     : the event time.
 *
 * Invoked by derived classes (and only by derived classes!) whenever the user
 * requests to open a context menu, e.g. by right-clicking on a file/folder or by
 * using one of the context menu shortcuts.
 **/
void
thunar_standard_view_context_menu (ThunarStandardView *standard_view,
                                   guint               button,
                                   guint32             timestamp)
{
  GtkWidget *menu;
  GList     *selected_items;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* merge the custom menu actions for the selected items */
  selected_items = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  thunar_standard_view_merge_custom_actions (standard_view, selected_items);
  g_list_free_full (selected_items, (GDestroyNotify) gtk_tree_path_free);

  /* grab an additional reference on the view */
  g_object_ref (G_OBJECT (standard_view));

  /* run the menu on the view's screen (figuring out whether to use the file or the folder context menu) */
  menu = gtk_ui_manager_get_widget (standard_view->ui_manager, (selected_items != NULL) ? "/file-context-menu" : "/folder-context-menu");
  thunar_gtk_menu_run (GTK_MENU (menu), GTK_WIDGET (standard_view), NULL, NULL, button, timestamp);

  /* release the additional reference on the view */
  g_object_unref (G_OBJECT (standard_view));
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

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (event != NULL);

  /* check if we have already scheduled a drag timer */
  if (G_LIKELY (standard_view->priv->drag_timer_id == 0))
    {
      /* remember the new coordinates */
      standard_view->priv->drag_x = event->x;
      standard_view->priv->drag_y = event->y;

      /* figure out the real view */
      view = GTK_BIN (standard_view)->child;

      /* we use the menu popup delay here, note that we only use this to
       * allow higher values! see bug #3549 */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (view));
      g_object_get (G_OBJECT (settings), "gtk-menu-popup-delay", &delay, NULL);

      /* schedule the timer */
      standard_view->priv->drag_timer_id = g_timeout_add_full (G_PRIORITY_LOW, MAX (225, delay), thunar_standard_view_drag_timer,
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
  gboolean    restorable;
  gboolean    pastable;
  gboolean    writable;
  gboolean    trashed;
  GList      *lp, *selected_files;
  gint        n_selected_files = 0;

  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));

  /* drop any existing "new-files" closure */
  if (G_UNLIKELY (standard_view->priv->new_files_closure != NULL))
    {
      g_closure_invalidate (standard_view->priv->new_files_closure);
      g_closure_unref (standard_view->priv->new_files_closure);
      standard_view->priv->new_files_closure = NULL;
    }

  /* release the previously selected files */
  thunar_g_file_list_free (standard_view->priv->selected_files);

  /* determine the new list of selected files (replacing GtkTreePath's with ThunarFile's) */
  selected_files = (*THUNAR_STANDARD_VIEW_GET_CLASS (standard_view)->get_selected_items) (standard_view);
  restorable = (selected_files != NULL);
  for (lp = selected_files; lp != NULL; lp = lp->next, ++n_selected_files)
    {
      /* determine the iterator for the path */
      gtk_tree_model_get_iter (GTK_TREE_MODEL (standard_view->model), &iter, lp->data);

      /* release the tree path... */
      gtk_tree_path_free (lp->data);

      /* ...and replace it with the file */
      lp->data = thunar_list_model_get_file (standard_view->model, &iter);

      /* enable "Restore" if we have only trashed files (atleast one file) */
      if (!thunar_file_is_trashed (lp->data))
        restorable = FALSE;
    }

  /* and setup the new selected files list */
  standard_view->priv->selected_files = selected_files;

  /* check whether the folder displayed by the view is writable/in the trash */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (standard_view));
  writable = (current_directory != NULL && thunar_file_is_writable (current_directory));
  trashed = (current_directory != NULL && thunar_file_is_trashed (current_directory));

  /* check whether the clipboard contains data that can be pasted here */
  pastable = (standard_view->clipboard != NULL && thunar_clipboard_manager_get_can_paste (standard_view->clipboard));

  /* check whether the only selected file is
   * folder to which we can paste to */
  can_paste_into_folder = (n_selected_files == 1)
                       && thunar_file_is_directory (selected_files->data)
                       && thunar_file_is_writable (selected_files->data);

  /* update the "Create Folder"/"Create Document" actions */
  gtk_action_set_sensitive (standard_view->priv->action_create_folder, !trashed && writable);
  gtk_action_set_sensitive (standard_view->priv->action_create_document, !trashed && writable);

  /* update the "Properties" action */
  gtk_action_set_sensitive (standard_view->priv->action_properties,
                            current_directory != NULL || n_selected_files > 0);

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

  /* update the "Move to Trash" action */
  g_object_set (G_OBJECT (standard_view->priv->action_move_to_trash),
                "sensitive", (n_selected_files > 0) && writable,
                "visible", !trashed && thunar_g_vfs_is_uri_scheme_supported ("trash"),
                "tooltip", ngettext ("Move the selected file to the Trash",
                                     "Move the selected files to the Trash",
                                     n_selected_files),
                NULL);

  /* update the "Delete" action */
  g_object_set (G_OBJECT (standard_view->priv->action_delete),
                "sensitive", (n_selected_files > 0) && writable,
                "tooltip", ngettext ("Permanently delete the selected file",
                                     "Permanently delete the selected files",
                                     n_selected_files),
                NULL);

  /* update the "Paste Into Folder" action */
  g_object_set (G_OBJECT (standard_view->priv->action_paste_into_folder),
                "sensitive", pastable,
                "visible", can_paste_into_folder,
                NULL);

  /* update the "Duplicate File(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_duplicate),
                "sensitive", (n_selected_files > 0) && !restorable && writable,
                "tooltip", ngettext ("Duplicate the selected file",
                                     "Duplicate each selected file",
                                     n_selected_files),
                NULL);

  /* update the "Make Link(s)" action */
  g_object_set (G_OBJECT (standard_view->priv->action_make_link),
                "label", ngettext ("Ma_ke Link", "Ma_ke Links", n_selected_files),
                "sensitive", (n_selected_files > 0) && !restorable && writable,
                "tooltip", ngettext ("Create a symbolic link for the selected file",
                                     "Create a symbolic link for each selected file",
                                     n_selected_files),
                NULL);

  /* update the "Rename" action */
  g_object_set (G_OBJECT (standard_view->priv->action_rename),
                "sensitive", (n_selected_files > 0 && !restorable && writable),
                "tooltip", ngettext ("Rename the selected file",
                                     "Rename the selected files",
                                     n_selected_files),
                NULL);

  /* update the "Restore" action */
  g_object_set (G_OBJECT (standard_view->priv->action_restore),
                "sensitive", (n_selected_files > 0 && restorable),
                "tooltip", ngettext ("Restore the selected file",
                                     "Restore the selected files",
                                     n_selected_files),
                NULL);

  /* update the statusbar text */
  thunar_standard_view_update_statusbar_text (standard_view);

  /* emit notification for "selected-files" */
  g_object_notify_by_pspec (G_OBJECT (standard_view), standard_view_props[PROP_SELECTED_FILES]);
}



void
thunar_standard_view_set_history (ThunarStandardView *standard_view,
                                  ThunarHistory      *history)
{
  _thunar_return_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view));
  _thunar_return_if_fail (history == NULL || THUNAR_IS_HISTORY (history));

  /* set the new history */
  g_object_unref (standard_view->priv->history);
  standard_view->priv->history = history;

  /* connect callback */
  g_signal_connect_swapped (G_OBJECT (history), "change-directory", G_CALLBACK (thunar_navigator_change_directory), standard_view);

  /* make the history use the action group of this view */
  g_object_set (G_OBJECT (history), "action-group", standard_view->action_group, NULL);
}



ThunarHistory *
thunar_standard_view_copy_history (ThunarStandardView *standard_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_STANDARD_VIEW (standard_view), NULL);

  return thunar_history_copy (standard_view->priv->history, NULL);
}
