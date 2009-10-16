/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-enum-types.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DEFAULT_VIEW,
  PROP_LAST_COMPACT_VIEW_ZOOM_LEVEL,
  PROP_LAST_DETAILS_VIEW_COLUMN_ORDER,
  PROP_LAST_DETAILS_VIEW_COLUMN_WIDTHS,
  PROP_LAST_DETAILS_VIEW_FIXED_COLUMNS,
  PROP_LAST_DETAILS_VIEW_VISIBLE_COLUMNS,
  PROP_LAST_DETAILS_VIEW_ZOOM_LEVEL,
  PROP_LAST_ICON_VIEW_ZOOM_LEVEL,
  PROP_LAST_LOCATION_BAR,
  PROP_LAST_SEPARATOR_POSITION,
  PROP_LAST_SHOW_HIDDEN,
  PROP_LAST_SIDE_PANE,
  PROP_LAST_SORT_COLUMN,
  PROP_LAST_SORT_ORDER,
  PROP_LAST_STATUSBAR_VISIBLE,
  PROP_LAST_VIEW,
  PROP_LAST_WINDOW_HEIGHT,
  PROP_LAST_WINDOW_WIDTH,
  PROP_LAST_WINDOW_FULLSCREEN,
  PROP_MISC_VOLUME_MANAGEMENT,
  PROP_MISC_CASE_SENSITIVE,
  PROP_MISC_DATE_STYLE,
  PROP_MISC_FOLDERS_FIRST,
  PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES,
  PROP_MISC_RECURSIVE_PERMISSIONS,
  PROP_MISC_REMEMBER_GEOMETRY,
  PROP_MISC_SHOW_ABOUT_TEMPLATES,
  PROP_MISC_SHOW_THUMBNAILS,
  PROP_MISC_SINGLE_CLICK,
  PROP_MISC_SINGLE_CLICK_TIMEOUT,
  PROP_MISC_TEXT_BESIDE_ICONS,
  PROP_SHORTCUTS_ICON_EMBLEMS,
  PROP_SHORTCUTS_ICON_SIZE,
  PROP_TREE_ICON_EMBLEMS,
  PROP_TREE_ICON_SIZE,
  N_PROPERTIES,
};



static void     thunar_preferences_finalize           (GObject                *object);
static void     thunar_preferences_get_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       GValue                 *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_set_property       (GObject                *object,
                                                       guint                   prop_id,
                                                       const GValue           *value,
                                                       GParamSpec             *pspec);
static void     thunar_preferences_resume_monitor     (ThunarPreferences      *preferences);
static void     thunar_preferences_suspend_monitor    (ThunarPreferences      *preferences);
static void     thunar_preferences_monitor            (GFileMonitor           *monitor,
                                                       GFile                  *file,
                                                       GFile                  *other_file,
                                                       GFileMonitorEvent       event_type,
                                                       gpointer                user_data);
static void     thunar_preferences_queue_load         (ThunarPreferences      *preferences);
static void     thunar_preferences_queue_store        (ThunarPreferences      *preferences);
static gboolean thunar_preferences_load_idle          (gpointer                user_data);
static void     thunar_preferences_load_idle_destroy  (gpointer                user_data);
static gboolean thunar_preferences_store_idle         (gpointer                user_data);
static void     thunar_preferences_store_idle_destroy (gpointer                user_data);



struct _ThunarPreferencesClass
{
  GObjectClass __parent__;
};

struct _ThunarPreferences
{
  GObject __parent__;

  GFileMonitor *monitor;

  GValue        values[N_PROPERTIES];

  gboolean      loading_in_progress;

  gint          load_idle_id;
  gint          store_idle_id;
};



G_DEFINE_TYPE (ThunarPreferences, thunar_preferences, G_TYPE_OBJECT)



static void
thunar_preferences_class_init (ThunarPreferencesClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_preferences_finalize;
  gobject_class->get_property = thunar_preferences_get_property;
  gobject_class->set_property = thunar_preferences_set_property;

  /**
   * ThunarPreferences:default-view:
   *
   * The name of the widget class, which should be used for the
   * view pane in new #ThunarWindow<!---->s or "void" to use the
   * last selected view from the "last-view" preference.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DEFAULT_VIEW,
                                   g_param_spec_string ("default-view",
                                                        "DefaultView",
                                                        "default-view",
                                                        "void",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-compact-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarCompactView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_COMPACT_VIEW_ZOOM_LEVEL,
                                   g_param_spec_enum ("last-compact-view-zoom-level",
                                                      "LastCompactViewZoomLevel",
                                                      "last-compact-view-zoom-level",
                                                      THUNAR_TYPE_ZOOM_LEVEL,
                                                      THUNAR_ZOOM_LEVEL_SMALLEST,
                                                      EXO_PARAM_READWRITE));


  /**
   * ThunarPreferences:last-details-view-column-order:
   *
   * The comma separated list of columns that specifies the order of the
   * columns in the #ThunarDetailsView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_DETAILS_VIEW_COLUMN_ORDER,
                                   g_param_spec_string ("last-details-view-column-order",
                                                        "LastDetailsViewColumnOrder",
                                                        "last-details-view-column-order",
                                                        "THUNAR_COLUMN_NAME,THUNAR_COLUMN_SIZE,THUNAR_COLUMN_TYPE,THUNAR_COLUMN_DATE_MODIFIED",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-details-view-column-widths:
   *
   * The comma separated list of column widths used for fixed width
   * #ThunarDetailsView<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_DETAILS_VIEW_COLUMN_WIDTHS,
                                   g_param_spec_string ("last-details-view-column-widths",
                                                        "LastDetailsViewColumnWidths",
                                                        "last-details-view-column-widths",
                                                        "",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-details-view-fixed-columns:
   *
   * %TRUE to use fixed column widths in the #ThunarDetailsView. Else the
   * column widths will be automatically determined from the model contents.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_DETAILS_VIEW_FIXED_COLUMNS,
                                   g_param_spec_boolean ("last-details-view-fixed-columns",
                                                         "LastDetailsViewFixedColumns",
                                                         "last-details-view-fixed-columns",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-details-view-visible-columns:
   *
   * The comma separated list of visible columns in the #ThunarDetailsView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_DETAILS_VIEW_VISIBLE_COLUMNS,
                                   g_param_spec_string ("last-details-view-visible-columns",
                                                        "LastDetailsViewVisibleColumns",
                                                        "last-details-view-visible-columns",
                                                        "THUNAR_COLUMN_DATE_MODIFIED,THUNAR_COLUMN_NAME,THUNAR_COLUMN_SIZE,THUNAR_COLUMN_TYPE",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-details-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarDetailsView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_DETAILS_VIEW_ZOOM_LEVEL,
                                   g_param_spec_enum ("last-details-view-zoom-level",
                                                      "LastDetailsViewZoomLevel",
                                                      "last-details-view-zoom-level",
                                                      THUNAR_TYPE_ZOOM_LEVEL,
                                                      THUNAR_ZOOM_LEVEL_SMALLER,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-icon-view-zoom-level:
   *
   * The last selected #ThunarZoomLevel for the #ThunarIconView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_ICON_VIEW_ZOOM_LEVEL,
                                   g_param_spec_enum ("last-icon-view-zoom-level",
                                                      "LastIconViewZoomLevel",
                                                      "last-icon-view-zoom-level",
                                                      THUNAR_TYPE_ZOOM_LEVEL,
                                                      THUNAR_ZOOM_LEVEL_NORMAL,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-location-bar:
   *
   * The name of the widget class, which should be used for the
   * location bar in #ThunarWindow<!---->s or "void" to hide the
   * location bar.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_LOCATION_BAR,
                                   g_param_spec_string ("last-location-bar",
                                                        "LastLocationBar",
                                                        "last-location-bar",
                                                        "ThunarLocationButtons",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-separator-position:
   *
   * The last position of the gutter in the main window,
   * which separates the side pane from the main view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SEPARATOR_POSITION,
                                   g_param_spec_int ("last-separator-position",
                                                     "LastSeparatorPosition",
                                                     "last-separator-position",
                                                     0, G_MAXINT, 170,
                                                     EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-show-hidden:
   *
   * Whether to show hidden files by default in new windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SHOW_HIDDEN,
                                   g_param_spec_boolean ("last-show-hidden",
                                                         "LastShowHidden",
                                                         "last-show-hidden",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-side-pane:
   *
   * The name of the widget class, which should be used for the
   * side pane in #ThunarWindow<!---->s or "void" to hide the
   * side pane completely.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SIDE_PANE,
                                   g_param_spec_string ("last-side-pane",
                                                        "LastSidePane",
                                                        "last-side-pane",
                                                        "ThunarShortcutsPane",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-sort-column:
   *
   * The default sort column for new views.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SORT_COLUMN,
                                   g_param_spec_enum ("last-sort-column",
                                                      "LastSortColumn",
                                                      "last-sort-column",
                                                      THUNAR_TYPE_COLUMN,
                                                      THUNAR_COLUMN_NAME,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-sort-order:
   *
   * The default sort order for new views.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_SORT_ORDER,
                                   g_param_spec_enum ("last-sort-order",
                                                      "LastSortOrder",
                                                      "last-sort-order",
                                                      GTK_TYPE_SORT_TYPE,
                                                      GTK_SORT_ASCENDING,
                                                      EXO_PARAM_READWRITE));
  /**
   * ThunarPreferences:last-statusbar-visible:
   *
   * Whether to display a statusbar in new windows by default.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_STATUSBAR_VISIBLE,
                                   g_param_spec_boolean ("last-statusbar-visible",
                                                         "LastStatusbarVisible",
                                                         "last-statusbar-visible",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-view:
   *
   * The name of the widget class, which should be used for the
   * main view component in #ThunarWindow<!---->s.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_VIEW,
                                   g_param_spec_string ("last-view",
                                                        "LastView",
                                                        "last-view",
                                                        "ThunarIconView",
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-window-height:
   *
   * The last known height of a #ThunarWindow, which will be used as
   * default height for newly created windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WINDOW_HEIGHT,
                                   g_param_spec_int ("last-window-height",
                                                     "LastWindowHeight",
                                                     "last-window-height",
                                                     1, G_MAXINT, 480,
                                                     EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:last-window-width:
   *
   * The last known width of a #ThunarWindow, which will be used as
   * default width for newly created windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WINDOW_WIDTH,
                                   g_param_spec_int ("last-window-width",
                                                     "LastWindowWidth",
                                                     "last-window-width",
                                                     1, G_MAXINT, 640,
                                                     EXO_PARAM_READWRITE));
                                                     
  /**
   * ThunarPreferences:last-window-maximized:
   *
   * The last known maximized state of a #ThunarWindow, which will be used as
   * default width for newly created windows.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LAST_WINDOW_FULLSCREEN,
                                   g_param_spec_boolean ("last-window-maximized",
                                                         "LastWindowMaximized",
                                                         "last-window-maximized",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-volume-management:
   *
   * Whether to enable volume management capabilities (requires HAL and the
   * thunar-volman package).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_VOLUME_MANAGEMENT,
                                   g_param_spec_boolean ("misc-volume-management",
                                                         "MiscVolumeManagement",
                                                         "misc-volume-management",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-case-sensitive:
   *
   * Whether to use case-sensitive sort.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_CASE_SENSITIVE,
                                   g_param_spec_boolean ("misc-case-sensitive",
                                                         "MiscCaseSensitive",
                                                         "misc-case-sensitive",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-date-style:
   *
   * The style used to display dates in the user interface.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_DATE_STYLE,
                                   g_param_spec_enum ("misc-date-style",
                                                      "MiscDateStyle",
                                                      "misc-date-style",
                                                      THUNAR_TYPE_DATE_STYLE,
                                                      THUNAR_DATE_STYLE_SIMPLE,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-folders-first:
   *
   * Whether to sort folders before files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_FOLDERS_FIRST,
                                   g_param_spec_boolean ("misc-folders-first",
                                                         "MiscFoldersFirst",
                                                         "misc-folders-first",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-horizontal-wheel-navigates:
   *
   * Whether the horizontal mouse wheel is used to navigate
   * forth and back within a Thunar view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_HORIZONTAL_WHEEL_NAVIGATES,
                                   g_param_spec_boolean ("misc-horizontal-wheel-navigates",
                                                         "MiscHorizontalWheelNavigates",
                                                         "misc-horizontal-wheel-navigates",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-recursive-permissions:
   *
   * Whether to apply permissions recursively everytime the
   * permissions are altered by the user.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_RECURSIVE_PERMISSIONS,
                                   g_param_spec_enum ("misc-recursive-permissions",
                                                      "MiscRecursivePermissions",
                                                      "misc-recursive-permissions",
                                                      THUNAR_TYPE_RECURSIVE_PERMISSIONS,
                                                      THUNAR_RECURSIVE_PERMISSIONS_ASK,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-remember-geometry:
   *
   * Whether Thunar should remember the size of windows and apply
   * that size to new windows. If %TRUE the width and height are
   * saved to "last-window-width" and "last-window-height". If
   * %FALSE the user may specify the start size in "last-window-with"
   * and "last-window-height".
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_REMEMBER_GEOMETRY,
                                   g_param_spec_boolean ("misc-remember-geometry",
                                                         "MiscRememberGeometry",
                                                         "misc-remember-geometry",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-show-about-templates:
   *
   * Whether to display the "About Templates" dialog, when opening the
   * Templates folder from the Go menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_SHOW_ABOUT_TEMPLATES,
                                   g_param_spec_boolean ("misc-show-about-templates",
                                                         "MiscShowAboutTemplates",
                                                         "misc-show-about-templates",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-show-thumbnails:
   *
   * Whether to generate and display thumbnails for previewable files.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_SHOW_THUMBNAILS,
                                   g_param_spec_boolean ("misc-show-thumbnails",
                                                         "MiscShowThumbnails",
                                                         "misc-show-thumbnails",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-single-click:
   *
   * Whether to use single click navigation.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_SINGLE_CLICK,
                                   g_param_spec_boolean ("misc-single-click",
                                                         "MiscSingleClick",
                                                         "misc-single-click",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-single-click-timeout:
   *
   * If single-click mode is enabled this is the amount of time
   * in milliseconds after which the item under the mouse cursor
   * will be selected automatically. A value of %0 disables the
   * automatic selection.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_SINGLE_CLICK_TIMEOUT,
                                   g_param_spec_uint ("misc-single-click-timeout",
                                                      "MiscSingleClickTimeout",
                                                      "misc-single-click-timeout",
                                                      0u, G_MAXUINT, 500u,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:misc-text-beside-icons:
   *
   * Whether the icon view should display the file names beside the
   * file icons instead of below the file icons.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MISC_TEXT_BESIDE_ICONS,
                                   g_param_spec_boolean ("misc-text-beside-icons",
                                                         "MiscTextBesideIcons",
                                                         "misc-text-beside-icons",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:shortcuts-icon-emblems:
   *
   * Whether to display emblems for file icons (if defined) in the
   * shortcuts side pane.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHORTCUTS_ICON_EMBLEMS,
                                   g_param_spec_boolean ("shortcuts-icon-emblems",
                                                         "ShortcutsIconEmblems",
                                                         "shortcuts-icon-emblems",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:shortcuts-icon-size:
   *
   * The icon size to use for the icons displayed in the
   * shortcuts side pane.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SHORTCUTS_ICON_SIZE,
                                   g_param_spec_enum ("shortcuts-icon-size",
                                                      "ShortcutsIconSize",
                                                      "shortcuts-icon-size",
                                                      THUNAR_TYPE_ICON_SIZE,
                                                      THUNAR_ICON_SIZE_SMALLER,
                                                      EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:tree-icon-emblems:
   *
   * Whether to display emblems for file icons (if defined) in the
   * tree side pane.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TREE_ICON_EMBLEMS,
                                   g_param_spec_boolean ("tree-icon-emblems",
                                                         "TreeIconEmblems",
                                                         "tree-icon-emblems",
                                                         TRUE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPreferences:tree-icon-size:
   *
   * The icon size to use for the icons displayed in the
   * tree side pane.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TREE_ICON_SIZE,
                                   g_param_spec_enum ("tree-icon-size",
                                                      "TreeIconSize",
                                                      "tree-icon-size",
                                                      THUNAR_TYPE_ICON_SIZE,
                                                      THUNAR_ICON_SIZE_SMALLEST,
                                                      EXO_PARAM_READWRITE));
}



static void
thunar_preferences_init (ThunarPreferences *preferences)
{
  preferences->monitor = NULL;

  /* load the settings */
  thunar_preferences_load_idle (preferences);

  /* launch the file monitor */
  thunar_preferences_resume_monitor (preferences);
}



static void
thunar_preferences_finalize (GObject *object)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  guint              n;

  /* flush preferences */
  if (G_UNLIKELY (preferences->store_idle_id != 0))
    {
      thunar_preferences_store_idle (preferences);
      g_source_remove (preferences->store_idle_id);
    }

  /* stop any pending load idle source */
  if (G_UNLIKELY (preferences->load_idle_id != 0))
    g_source_remove (preferences->load_idle_id);

  /* stop the file monitor */
  if (G_LIKELY (preferences->monitor != NULL))
    {
      thunar_preferences_suspend_monitor (preferences);
      g_object_unref (G_OBJECT (preferences->monitor));
    }

  /* release the property values */
  for (n = 1; n < N_PROPERTIES; ++n)
    if (G_IS_VALUE (preferences->values + n))
      g_value_unset (preferences->values + n);

  (*G_OBJECT_CLASS (thunar_preferences_parent_class)->finalize) (object);
}



static void
thunar_preferences_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  GValue            *src;

  src = preferences->values + prop_id;
  if (G_IS_VALUE (src))
    g_value_copy (src, value);
  else
    g_param_value_set_default (pspec, value);
}



static void
thunar_preferences_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (object);
  GValue            *dst;

  dst = preferences->values + prop_id;
  if (G_UNLIKELY (!G_IS_VALUE (dst)))
    {
      g_value_init (dst, pspec->value_type);
      g_param_value_set_default (pspec, dst);
    }

  if (g_param_values_cmp (pspec, value, dst) != 0)
    {
      g_value_copy (value, dst);
      thunar_preferences_queue_store (preferences);
    }
}



static void
thunar_preferences_resume_monitor (ThunarPreferences *preferences)
{
  GFile *file;
  gchar *filename;

  /* verify that the monitor is suspended */
  if (G_LIKELY (preferences->monitor == NULL))
    {
      /* determine the save location for thunarrc to monitor */
      filename = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", TRUE);
      if (G_LIKELY (filename != NULL))
        {
          /* monitor this file */
          file = g_file_new_for_path (filename);
          preferences->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
          if (G_LIKELY (preferences->monitor != NULL))
            g_signal_connect (preferences->monitor, "changed", G_CALLBACK (thunar_preferences_monitor), preferences);
          g_object_unref (file);

          /* release the filename */
          g_free (filename);
        }
    }
}



static void
thunar_preferences_suspend_monitor (ThunarPreferences *preferences)
{
  /* verify that the monitor is active */
  if (G_LIKELY (preferences->monitor != NULL 
                && !g_file_monitor_is_cancelled (preferences->monitor)))
    {
      /* disconnect the handle from the monitor */
      g_file_monitor_cancel (preferences->monitor);
    }
}



static void
thunar_preferences_monitor (GFileMonitor           *monitor,
                            GFile                  *file,
                            GFile                  *other_file,
                            GFileMonitorEvent       event_type,
                            gpointer                user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);

  _thunar_return_if_fail (THUNAR_IS_PREFERENCES (preferences));
  _thunar_return_if_fail (G_IS_FILE_MONITOR (monitor));
  _thunar_return_if_fail (preferences->monitor == monitor);

  /* schedule a reload whenever the file is created/changed */
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED 
      || event_type == G_FILE_MONITOR_EVENT_CREATED)
    {
      thunar_preferences_queue_load (preferences);
    }
}



static void
thunar_preferences_queue_load (ThunarPreferences *preferences)
{
  if (preferences->load_idle_id == 0 && preferences->store_idle_id == 0)
    {
      preferences->load_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_preferences_load_idle,
                                                   preferences, thunar_preferences_load_idle_destroy);
    }
}



static void
thunar_preferences_queue_store (ThunarPreferences *preferences)
{
  if (preferences->store_idle_id == 0 && !preferences->loading_in_progress)
    {
      preferences->store_idle_id = g_idle_add_full (G_PRIORITY_LOW, thunar_preferences_store_idle,
                                                    preferences, thunar_preferences_store_idle_destroy);
    }
}



#ifndef NDEBUG
static gchar*
property_name_to_option_name (const gchar *property_name)
{
  const gchar *s;
  gboolean     upper = TRUE;
  gchar       *option;
  gchar       *t;

  option = g_new (gchar, strlen (property_name) + 1);
  for (s = property_name, t = option; *s != '\0'; ++s)
    {
      if (*s == '-')
        {
          upper = TRUE;
        }
      else if (upper)
        {
          *t++ = g_ascii_toupper (*s);
          upper = FALSE;
        }
      else
        {
          *t++ = *s;
        }
    }
  *t = '\0';

  return option;
}
#endif



static gboolean
thunar_preferences_load_idle (gpointer user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);
  const gchar       *string;
  GParamSpec       **specs;
  GParamSpec        *spec;
  XfceRc            *rc;
  GValue             dst = { 0, };
  GValue             src = { 0, };
#ifndef NDEBUG
  gchar             *option;
#endif
  guint              nspecs;
  guint              n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", TRUE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Failed to load thunar preferences.");
      return FALSE;
    }

  g_object_freeze_notify (G_OBJECT (preferences));

  xfce_rc_set_group (rc, "Configuration");

  preferences->loading_in_progress = TRUE;

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);
  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

#ifndef NDEBUG
      /* when debugging is enabled, check if the generated option name
       * is equal to the nickname, to prevent typos */
      option = property_name_to_option_name (spec->name);
      g_assert (exo_str_is_equal (option, g_param_spec_get_nick (spec)));
      g_free (option);
#endif

      string = xfce_rc_read_entry (rc, g_param_spec_get_nick (spec), NULL);

      if (G_UNLIKELY (string == NULL))
        continue;

      g_value_init (&src, G_TYPE_STRING);
      g_value_set_static_string (&src, string);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_set_property (G_OBJECT (preferences), spec->name, &src);
        }
      else if (g_value_type_transformable (G_TYPE_STRING, spec->value_type))
        {
          g_value_init (&dst, spec->value_type);
          if (g_value_transform (&src, &dst))
            g_object_set_property (G_OBJECT (preferences), spec->name, &dst);
          g_value_unset (&dst);
        }
      else
        {
          g_warning ("Failed to load property \"%s\"", spec->name);
        }

      g_value_unset (&src);
    }
  g_free (specs);

  preferences->loading_in_progress = FALSE;

  xfce_rc_close (rc);

  g_object_thaw_notify (G_OBJECT (preferences));

  return FALSE;
}



static void
thunar_preferences_load_idle_destroy (gpointer user_data)
{
  THUNAR_PREFERENCES (user_data)->load_idle_id = 0;
}



static gboolean
thunar_preferences_store_idle (gpointer user_data)
{
  ThunarPreferences *preferences = THUNAR_PREFERENCES (user_data);
  const gchar       *string;
  GParamSpec       **specs;
  GParamSpec        *spec;
  XfceRc            *rc;
  GValue             dst = { 0, };
  GValue             src = { 0, };
#ifndef NDEBUG
  gchar             *option;
#endif
  guint              nspecs;
  guint              n;

  rc = xfce_rc_config_open (XFCE_RESOURCE_CONFIG, "Thunar/thunarrc", FALSE);
  if (G_UNLIKELY (rc == NULL))
    {
      g_warning ("Failed to store thunar preferences.");
      return FALSE;
    }

  /* suspend the monitor (hopefully tricking FAM to avoid unnecessary reloads) */
  thunar_preferences_suspend_monitor (preferences);

  xfce_rc_set_group (rc, "Configuration");

  specs = g_object_class_list_properties (G_OBJECT_GET_CLASS (preferences), &nspecs);
  for (n = 0; n < nspecs; ++n)
    {
      spec = specs[n];

      g_value_init (&dst, G_TYPE_STRING);

      if (spec->value_type == G_TYPE_STRING)
        {
          g_object_get_property (G_OBJECT (preferences), spec->name, &dst);
        }
      else
        {
          g_value_init (&src, spec->value_type);
          g_object_get_property (G_OBJECT (preferences), spec->name, &src);
          g_value_transform (&src, &dst);
          g_value_unset (&src);
        }

#ifndef NDEBUG
      /* when debugging is enabled, check if the generated option name
       * is equal to the nickname, to prevent typos */
      option = property_name_to_option_name (spec->name);
      g_assert (exo_str_is_equal (option, g_param_spec_get_nick (spec)));
      g_free (option);
#endif

      /* store the setting */
      string = g_value_get_string (&dst);
      if (G_LIKELY (string != NULL))
        xfce_rc_write_entry (rc, g_param_spec_get_nick (spec), string);

      /* cleanup */
      g_value_unset (&dst);
    }

  /* cleanup */
  xfce_rc_close (rc);
  g_free (specs);

  /* restart the monitor */
  thunar_preferences_resume_monitor (preferences);

  return FALSE;
}



static void
thunar_preferences_store_idle_destroy (gpointer user_data)
{
  THUNAR_PREFERENCES (user_data)->store_idle_id = 0;
}



/**
 * thunar_preferences_get:
 *
 * Queries the global #ThunarPreferences instance, which is shared
 * by all modules. The function automatically takes a reference
 * for the caller, so you'll need to call g_object_unref() when
 * you're done with it.
 *
 * Return value: the global #ThunarPreferences instance.
 **/
ThunarPreferences*
thunar_preferences_get (void)
{
  static ThunarPreferences *preferences = NULL;

  if (G_UNLIKELY (preferences == NULL))
    {
      preferences = g_object_new (THUNAR_TYPE_PREFERENCES, NULL);
      g_object_add_weak_pointer (G_OBJECT (preferences),
                                 (gpointer) &preferences);
    }
  else
    {
      g_object_ref (G_OBJECT (preferences));
    }

  return preferences;
}


