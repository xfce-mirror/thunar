/* vi:set et ai sw=2 sts=2 ts=2: */
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
#include "config.h"
#endif

#include "thunar/thunar-action-manager.h"
#include "thunar/thunar-column-editor.h"
#include "thunar/thunar-details-view.h"
#include "thunar/thunar-gtk-extensions.h"
#include "thunar/thunar-preferences.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-text-renderer.h"
#include "thunar/thunar-window.h"

#include <gdk/gdkkeysyms.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FIXED_COLUMNS,
  PROP_EXPANDABLE_FOLDERS,
};



static void
thunar_details_view_finalize (GObject *object);
static void
thunar_details_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec);
static void
thunar_details_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec);
static AtkObject *
thunar_details_view_get_accessible (GtkWidget *widget);
static GList *
thunar_details_view_get_selected_items (ThunarStandardView *standard_view);
static void
thunar_details_view_select_all (ThunarStandardView *standard_view);
static void
thunar_details_view_unselect_all (ThunarStandardView *standard_view);
static void
thunar_details_view_selection_invert (ThunarStandardView *standard_view);
static void
thunar_details_view_select_path (ThunarStandardView *standard_view,
                                 GtkTreePath        *path);
static void
thunar_details_view_set_cursor (ThunarStandardView *standard_view,
                                GtkTreePath        *path,
                                gboolean            start_editing);
static void
thunar_details_view_scroll_to_path (ThunarStandardView *standard_view,
                                    GtkTreePath        *path,
                                    gboolean            use_align,
                                    gfloat              row_align,
                                    gfloat              col_align);
static GtkTreePath *
thunar_details_view_get_path_at_pos (ThunarStandardView *standard_view,
                                     gint                x,
                                     gint                y);
static gboolean
thunar_details_view_get_visible_range (ThunarStandardView *standard_view,
                                       GtkTreePath       **start_path,
                                       GtkTreePath       **end_path);
static void
thunar_details_view_highlight_path (ThunarStandardView *standard_view,
                                    GtkTreePath        *path);
static void
thunar_details_view_notify_model (GtkTreeView       *tree_view,
                                  GParamSpec        *pspec,
                                  ThunarDetailsView *details_view);
static void
thunar_details_view_notify_width (GtkTreeViewColumn *tree_view_column,
                                  GParamSpec        *pspec,
                                  ThunarDetailsView *details_view);
static gboolean
thunar_details_view_column_header_clicked (ThunarDetailsView *details_view,
                                           GdkEventButton    *event);
static gboolean
thunar_details_view_button_press_event (GtkTreeView       *tree_view,
                                        GdkEventButton    *event,
                                        ThunarDetailsView *details_view);
static gboolean
thunar_details_view_key_press_event (GtkTreeView       *tree_view,
                                     GdkEventKey       *event,
                                     ThunarDetailsView *details_view);
static void
thunar_details_view_row_activated (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column,
                                   ThunarDetailsView *details_view);
static void
thunar_details_view_row_expanded (GtkTreeView       *tree_view,
                                  GtkTreeIter       *iter,
                                  GtkTreePath       *path,
                                  ThunarDetailsView *view);
static void
thunar_details_view_row_collapsed (GtkTreeView       *tree_view,
                                   GtkTreeIter       *iter,
                                   GtkTreePath       *path,
                                   ThunarDetailsView *view);
static gboolean
thunar_details_view_select_cursor_row (GtkTreeView       *tree_view,
                                       gboolean           editing,
                                       ThunarDetailsView *details_view);
static void
thunar_details_view_row_changed (GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column,
                                 ThunarDetailsView *details_view);
static void
thunar_details_view_columns_changed (ThunarColumnModel *column_model,
                                     ThunarDetailsView *details_view);
static void
thunar_details_view_zoom_level_changed (ThunarDetailsView *details_view);
static gboolean
thunar_details_view_get_fixed_columns (ThunarDetailsView *details_view);
static void
thunar_details_view_set_fixed_columns (ThunarDetailsView *details_view,
                                       gboolean           fixed_columns);
static void
thunar_details_view_set_expandable_folders (ThunarDetailsView *details_view,
                                            gboolean           expandable_folders);
static void
thunar_details_view_connect_accelerators (ThunarStandardView *standard_view,
                                          GtkAccelGroup      *accel_group);
static void
thunar_details_view_disconnect_accelerators (ThunarStandardView *standard_view,
                                             GtkAccelGroup      *accel_group);
static void
thunar_details_view_append_menu_items (ThunarStandardView *standard_view,
                                       GtkMenu            *menu,
                                       GtkAccelGroup      *accel_group);
static void
thunar_details_view_highlight_option_changed (ThunarDetailsView *details_view);
static void
thunar_details_view_queue_redraw (ThunarStandardView *standard_view);
static gboolean
thunar_details_view_toggle_expandable_folders (ThunarDetailsView *details_view);
static void
thunar_details_view_block_selection_changed (ThunarStandardView *standard_view);
static void
thunar_details_view_unblock_selection_changed (ThunarStandardView *standard_view);



struct _ThunarDetailsViewClass
{
  ThunarStandardViewClass __parent__;
};



struct _ThunarDetailsView
{
  ThunarStandardView __parent__;

  /* the column model */
  ThunarColumnModel *column_model;

  /* the tree view columns */
  GtkTreeViewColumn *columns[THUNAR_N_VISIBLE_COLUMNS];

  /* whether to use fixed column widths */
  gboolean fixed_columns;

  /* event source id for thunar_details_view_zoom_level_changed_reload_fixed_height */
  guint idle_id;

  GtkCellRenderer *renderers[THUNAR_N_VISIBLE_COLUMNS];

  ExoTreeView *tree_view;

  gboolean expandable_folders;
};



/* clang-format off */
static XfceGtkActionEntry thunar_details_view_action_entries[] =
{
    { THUNAR_DETAILS_VIEW_ACTION_CONFIGURE_COLUMNS,           "<Actions>/ThunarStandardView/configure-columns",         "", XFCE_GTK_MENU_ITEM ,       N_ ("Configure _Columns..."),      N_ ("Configure the columns in the detailed list view"), NULL, G_CALLBACK (thunar_show_column_editor),                     },
    { THUNAR_DETAILS_VIEW_ACTION_TOGGLE_EXPANDABLE_FOLDERS,   "<Actions>/ThunarDetailsView/expandable-folders",         "", XFCE_GTK_CHECK_MENU_ITEM,  N_ ("E_xpandable Folders"),        N_ ("Allow tree like expansion of folders"),            NULL, G_CALLBACK (thunar_details_view_toggle_expandable_folders), },
};
/* clang-format on */

#define get_action_entry(id) xfce_gtk_get_action_entry_by_id (thunar_details_view_action_entries, G_N_ELEMENTS (thunar_details_view_action_entries), id)



G_DEFINE_TYPE (ThunarDetailsView, thunar_details_view, THUNAR_TYPE_STANDARD_VIEW)



static void
thunar_details_view_class_init (ThunarDetailsViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;
  GObjectClass            *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_details_view_finalize;
  gobject_class->get_property = thunar_details_view_get_property;
  gobject_class->set_property = thunar_details_view_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_details_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->get_selected_items = thunar_details_view_get_selected_items;
  thunarstandard_view_class->select_all = thunar_details_view_select_all;
  thunarstandard_view_class->unselect_all = thunar_details_view_unselect_all;
  thunarstandard_view_class->selection_invert = thunar_details_view_selection_invert;
  thunarstandard_view_class->select_path = thunar_details_view_select_path;
  thunarstandard_view_class->set_cursor = thunar_details_view_set_cursor;
  thunarstandard_view_class->scroll_to_path = thunar_details_view_scroll_to_path;
  thunarstandard_view_class->get_path_at_pos = thunar_details_view_get_path_at_pos;
  thunarstandard_view_class->get_visible_range = thunar_details_view_get_visible_range;
  thunarstandard_view_class->highlight_path = thunar_details_view_highlight_path;
  thunarstandard_view_class->append_menu_items = thunar_details_view_append_menu_items;
  thunarstandard_view_class->connect_accelerators = thunar_details_view_connect_accelerators;
  thunarstandard_view_class->disconnect_accelerators = thunar_details_view_disconnect_accelerators;
  thunarstandard_view_class->queue_redraw = thunar_details_view_queue_redraw;
  thunarstandard_view_class->zoom_level_property_name = "last-details-view-zoom-level";
  thunarstandard_view_class->block_selection = thunar_details_view_block_selection_changed;
  thunarstandard_view_class->unblock_selection = thunar_details_view_unblock_selection_changed;

  xfce_gtk_translate_action_entries (thunar_details_view_action_entries, G_N_ELEMENTS (thunar_details_view_action_entries));

  /**
   * ThunarDetailsView:fixed-columns:
   *
   * %TRUE to use fixed column widths.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FIXED_COLUMNS,
                                   g_param_spec_boolean ("fixed-columns",
                                                         "fixed-columns",
                                                         "fixed-columns",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarDetailsView:tree-view:
   *
   * %TRUE to enable tree-view.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_EXPANDABLE_FOLDERS,
                                   g_param_spec_boolean ("expandable-folders",
                                                         "expandable-folders",
                                                         "expandable-folders",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
}



static void
thunar_details_view_init (ThunarDetailsView *details_view)
{
  GtkTreeSelection *selection;
  GtkCellRenderer  *right_aligned_renderer;
  GtkCellRenderer  *left_aligned_renderer;
  GtkCellRenderer  *renderer;
  ThunarColumn      column;

  /* we need to force the GtkTreeView to recalculate column sizes
   * whenever the zoom-level changes, so we connect a handler here.
   */
  g_signal_connect (G_OBJECT (details_view), "notify::zoom-level", G_CALLBACK (thunar_details_view_zoom_level_changed), NULL);

  /* create the tree view to embed */
  details_view->tree_view = EXO_TREE_VIEW (exo_tree_view_new ());
  g_signal_connect (G_OBJECT (details_view->tree_view), "notify::model",
                    G_CALLBACK (thunar_details_view_notify_model), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "button-press-event",
                    G_CALLBACK (thunar_details_view_button_press_event), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "key-press-event",
                    G_CALLBACK (thunar_details_view_key_press_event), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "row-activated",
                    G_CALLBACK (thunar_details_view_row_activated), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "select-cursor-row",
                    G_CALLBACK (thunar_details_view_select_cursor_row), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "row-expanded",
                    G_CALLBACK (thunar_details_view_row_expanded), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "row-collapsed",
                    G_CALLBACK (thunar_details_view_row_collapsed), details_view);
  g_signal_connect (G_OBJECT (details_view->tree_view), "select-cursor-row",
                    G_CALLBACK (thunar_details_view_select_cursor_row), details_view);
  gtk_container_add (GTK_CONTAINER (details_view), GTK_WIDGET (details_view->tree_view));
  gtk_widget_show (GTK_WIDGET (details_view->tree_view));

  /* configure general aspects of the details view */
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (details_view->tree_view), TRUE);

  /* enable rubberbanding (if supported) */
  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (details_view->tree_view), TRUE);

  /* tell standard view to use tree-model */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)), "model-type", THUNAR_TYPE_TREE_VIEW_MODEL, NULL);

  /* Bind tree-view property between view and preferences object */
  g_object_bind_property (THUNAR_STANDARD_VIEW (details_view)->preferences, "misc-expandable-folders",
                          G_OBJECT (details_view), "expandable-folders",
                          G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

  /* connect to the default column model */
  details_view->column_model = thunar_column_model_get_default ();
  g_signal_connect (G_OBJECT (details_view->column_model), "columns-changed", G_CALLBACK (thunar_details_view_columns_changed), details_view);
  g_signal_connect_after (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->model), "row-changed",
                          G_CALLBACK (thunar_details_view_row_changed), details_view);

  /* allocate the shared right-aligned text renderer */
  right_aligned_renderer = g_object_new (thunar_text_renderer_get_type (), "xalign", 1.0f, NULL);

  /* this is required in order to disable foreground & background colors on text renderers when the feature is disabled */
  g_object_bind_property (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences), "misc-highlighting-enabled",
                          G_OBJECT (right_aligned_renderer), "foreground-set",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences), "misc-highlighting-enabled",
                          G_OBJECT (right_aligned_renderer), "highlighting-enabled",
                          G_BINDING_SYNC_CREATE);
  g_object_ref_sink (G_OBJECT (right_aligned_renderer));

  /* allocate the shared left-aligned text renderer */
  left_aligned_renderer = g_object_new (thunar_text_renderer_get_type (), "xalign", 0.0f, NULL);

  /* this is required in order to disable foreground & background colors on text renderers when the feature is disabled */
  g_object_bind_property (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences), "misc-highlighting-enabled",
                          G_OBJECT (left_aligned_renderer), "foreground-set",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences), "misc-highlighting-enabled",
                          G_OBJECT (left_aligned_renderer), "highlighting-enabled",
                          G_BINDING_SYNC_CREATE);
  g_object_ref_sink (G_OBJECT (left_aligned_renderer));

  /* allocate the tree view columns */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
      /* allocate the tree view column */
      details_view->columns[column] = gtk_tree_view_column_new ();
      g_object_ref_sink (G_OBJECT (details_view->columns[column]));
      gtk_tree_view_column_set_min_width (details_view->columns[column], 50);
      gtk_tree_view_column_set_resizable (details_view->columns[column], TRUE);
      gtk_tree_view_column_set_sort_column_id (details_view->columns[column], column);
      gtk_tree_view_column_set_title (details_view->columns[column], thunar_column_model_get_column_name (details_view->column_model, column));

      /* stay informed whenever the width of the column is changed */
      g_signal_connect (G_OBJECT (details_view->columns[column]), "notify::width", G_CALLBACK (thunar_details_view_notify_width), details_view);

      /* name column gets special renderers */
      if (G_UNLIKELY (column == THUNAR_COLUMN_NAME))
        {
          /* add the icon renderer */
          gtk_tree_view_column_pack_start (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->icon_renderer, FALSE);
          gtk_tree_view_column_set_attributes (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->icon_renderer,
                                               "file", THUNAR_COLUMN_FILE,
                                               NULL);

          /* add the name renderer */
          g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->name_renderer),
                        "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, "width-chars", 60, NULL);
          gtk_tree_view_column_pack_start (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->name_renderer, TRUE);
          gtk_tree_view_column_set_attributes (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->name_renderer,
                                               "text", THUNAR_COLUMN_NAME,
                                               NULL);
        }
      else
        {
          /* size is right aligned, everything else is left aligned */
          renderer = (column == THUNAR_COLUMN_SIZE || column == THUNAR_COLUMN_SIZE_IN_BYTES) ? right_aligned_renderer : left_aligned_renderer;
          details_view->renderers[column] = renderer;

          /* add the renderer */
          gtk_tree_view_column_pack_start (details_view->columns[column], renderer, TRUE);
          gtk_tree_view_column_set_attributes (details_view->columns[column], renderer, "text", column, NULL);
        }

      /* append the tree view column to the tree view */
      gtk_tree_view_append_column (GTK_TREE_VIEW (details_view->tree_view), details_view->columns[column]);
    }

  /* configure the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (details_view->tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
                            G_CALLBACK (thunar_standard_view_selection_changed), details_view);

  /* apply the initial column order and visibility from the column model */
  thunar_details_view_columns_changed (details_view->column_model, details_view);

  /* synchronize the "fixed-columns" property */
  g_object_bind_property (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences),
                          "last-details-view-fixed-columns",
                          G_OBJECT (details_view),
                          "fixed-columns",
                          G_BINDING_SYNC_CREATE);

  /* apply fixed column widths if we start in fixed column mode */
  if (G_UNLIKELY (details_view->fixed_columns))
    {
      /* apply to all columns */
      for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
        gtk_tree_view_column_set_fixed_width (details_view->columns[column], thunar_column_model_get_column_width (details_view->column_model, column));
    }
  else
    {
      /* Make sure the name column auto-expands */
      gtk_tree_view_column_set_fixed_width (details_view->columns[THUNAR_COLUMN_NAME], -1);
      gtk_tree_view_column_set_expand (details_view->columns[THUNAR_COLUMN_NAME], TRUE);
    }

  g_signal_connect_swapped (THUNAR_STANDARD_VIEW (details_view)->preferences, "notify::misc-highlighting-enabled",
                            G_CALLBACK (thunar_details_view_highlight_option_changed), details_view);
  thunar_details_view_highlight_option_changed (details_view);

  /* release the shared text renderers */
  g_object_unref (G_OBJECT (right_aligned_renderer));
  g_object_unref (G_OBJECT (left_aligned_renderer));
}



static void
thunar_details_view_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FIXED_COLUMNS:
      g_value_set_boolean (value, thunar_details_view_get_fixed_columns (details_view));
      break;

    case PROP_EXPANDABLE_FOLDERS:
      g_value_set_boolean (value, details_view->expandable_folders);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_details_view_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (object);

  switch (prop_id)
    {
    case PROP_FIXED_COLUMNS:
      thunar_details_view_set_fixed_columns (details_view, g_value_get_boolean (value));
      break;

    case PROP_EXPANDABLE_FOLDERS:
      thunar_details_view_set_expandable_folders (details_view, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_details_view_finalize (GObject *object)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (object);
  ThunarColumn       column;

  /* release the tree view columns array */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    g_object_unref (G_OBJECT (details_view->columns[column]));

  /* disconnect from the default column model */
  g_signal_handlers_disconnect_by_func (G_OBJECT (details_view->column_model), thunar_details_view_columns_changed, details_view);
  g_object_unref (G_OBJECT (details_view->column_model));

  if (details_view->idle_id)
    g_source_remove (details_view->idle_id);

  g_signal_handlers_disconnect_by_func (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences),
                                        thunar_details_view_highlight_option_changed, details_view);

  (*G_OBJECT_CLASS (thunar_details_view_parent_class)->finalize) (object);
}



static AtkObject *
thunar_details_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the tree view class */
  object = (*GTK_WIDGET_CLASS (thunar_details_view_parent_class)->get_accessible) (widget);

  /* set custom Atk properties for the details view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_description (object, _("Detailed directory listing"));
      atk_object_set_name (object, _("Details view"));
      atk_object_set_role (object, ATK_ROLE_DIRECTORY_PANE);
    }

  return object;
}



static GList *
thunar_details_view_get_selected_items (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
  return gtk_tree_selection_get_selected_rows (selection, NULL);
}



static void
thunar_details_view_select_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
  gtk_tree_selection_select_all (selection);
}



static void
thunar_details_view_unselect_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
  gtk_tree_selection_unselect_all (selection);
}



static void
thunar_details_view_selection_invert_foreach (GtkTreeModel *model,
                                              GtkTreePath  *path,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
  GList **list = data;

  *list = g_list_prepend (*list, gtk_tree_path_copy (path));
}



static void
thunar_details_view_selection_invert (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;
  GList            *selected_paths = NULL;
  GList            *lp;
  GtkTreePath      *path;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));

  /* block updates */
  g_signal_handlers_block_by_func (selection, thunar_standard_view_selection_changed, standard_view);

  /* get paths of selected files */
  gtk_tree_selection_selected_foreach (selection, thunar_details_view_selection_invert_foreach, &selected_paths);

  gtk_tree_selection_select_all (selection);

  for (lp = selected_paths; lp != NULL; lp = lp->next)
    {
      path = lp->data;
      gtk_tree_selection_unselect_path (selection, path);
      gtk_tree_path_free (path);
    }

  g_list_free (selected_paths);

  /* unblock updates */
  g_signal_handlers_unblock_by_func (selection, thunar_standard_view_selection_changed, standard_view);

  thunar_standard_view_selection_changed (THUNAR_STANDARD_VIEW (standard_view));
}



static void
thunar_details_view_select_path (ThunarStandardView *standard_view,
                                 GtkTreePath        *path)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))));
  gtk_tree_selection_select_path (selection, path);
}



static void
thunar_details_view_set_cursor (ThunarStandardView *standard_view,
                                GtkTreePath        *path,
                                gboolean            start_editing)
{
  GtkCellRendererMode mode;
  GtkTreeViewColumn  *column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  /* make sure the name renderer is editable */
  g_object_get (G_OBJECT (standard_view->name_renderer), "mode", &mode, NULL);
  g_object_set (G_OBJECT (standard_view->name_renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);

  /* tell the tree view to start editing the given row */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), 0);
  gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))),
                                    path, column, standard_view->name_renderer,
                                    start_editing);

  /* reset the name renderer mode */
  g_object_set (G_OBJECT (standard_view->name_renderer), "mode", mode, NULL);
}



static void
thunar_details_view_scroll_to_path (ThunarStandardView *standard_view,
                                    GtkTreePath        *path,
                                    gboolean            use_align,
                                    gfloat              row_align,
                                    gfloat              col_align)
{
  GtkTreeViewColumn *column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  /* sometimes StandardView can trigger this before the model has been set in the view */
  if (gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view)))) == NULL)
    return;

  /* tell the tree view to scroll to the given row */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), 0);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, column, use_align, row_align, col_align);
}



static GtkTreePath *
thunar_details_view_get_path_at_pos (ThunarStandardView *standard_view,
                                     gint                x,
                                     gint                y)
{
  GtkTreePath *path;

  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), NULL);

  if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), x, y, &path, NULL))
    return path;

  return NULL;
}



static gboolean
thunar_details_view_get_visible_range (ThunarStandardView *standard_view,
                                       GtkTreePath       **start_path,
                                       GtkTreePath       **end_path)
{
  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), FALSE);

  return gtk_tree_view_get_visible_range (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), start_path, end_path);
}



static void
thunar_details_view_highlight_path (ThunarStandardView *standard_view,
                                    GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (standard_view))), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}



static void
thunar_details_view_notify_model (GtkTreeView       *tree_view,
                                  GParamSpec        *pspec,
                                  ThunarDetailsView *details_view)
{
  /* We need to set the search column here, as GtkTreeView resets it
   * whenever a new model is set.
   */
  gtk_tree_view_set_search_column (tree_view, THUNAR_COLUMN_NAME);
}



static void
thunar_details_view_notify_width (GtkTreeViewColumn *tree_view_column,
                                  GParamSpec        *pspec,
                                  ThunarDetailsView *details_view)
{
  ThunarColumn column;

  _thunar_return_if_fail (GTK_IS_TREE_VIEW_COLUMN (tree_view_column));
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  /* for some reason gtk forgets the auto-expand state of the name column */
  gtk_tree_view_column_set_expand (details_view->columns[THUNAR_COLUMN_NAME], !details_view->fixed_columns);

  /* lookup the column no for the given tree view column */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    if (details_view->columns[column] == tree_view_column)
      {
        /* save the new width as default fixed width */
        thunar_column_model_set_column_width (details_view->column_model, column, gtk_tree_view_column_get_width (tree_view_column));
        break;
      }
}



static gboolean
thunar_details_view_column_header_clicked (ThunarDetailsView *details_view,
                                           GdkEventButton    *event)
{
  GtkWidget *menu;

  if (event->button == 3)
    {
      menu = gtk_menu_new ();
      xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_DETAILS_VIEW_ACTION_CONFIGURE_COLUMNS), G_OBJECT (details_view), GTK_MENU_SHELL (menu));
      gtk_widget_show_all (menu);

      /* run the menu (takes over the floating of menu) */
      thunar_gtk_menu_run (GTK_MENU (menu));
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_details_view_button_press_event (GtkTreeView       *tree_view,
                                        GdkEventButton    *event,
                                        ThunarDetailsView *details_view)
{
  GtkTreeSelection  *selection;
  GtkTreePath       *path = NULL;
  GtkTreeViewColumn *column;
  GtkTreeViewColumn *name_column;
  GtkWidget         *window;
  GtkTreeModel      *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter        iter;
  ThunarFile        *file = NULL;
  gboolean           row_selected = FALSE;
  gint               expander_size, horizontal_separator;
  gboolean           on_expander = FALSE, activate_on_single_click = FALSE;
  GdkRectangle       rect;
  gint               edge_to_expander_region_width;

  /* check if the event is for the bin window */
  if (G_UNLIKELY (event->window != gtk_tree_view_get_bin_window (tree_view)))
    return thunar_details_view_column_header_clicked (details_view, event);

  /* give focus to the clicked view */
  window = gtk_widget_get_toplevel (GTK_WIDGET (details_view));
  thunar_window_focus_view (THUNAR_WINDOW (window), GTK_WIDGET (details_view));

  /* get the current selection */
  selection = gtk_tree_view_get_selection (tree_view);

  /* get the column showing the filenames */
  name_column = details_view->columns[THUNAR_COLUMN_NAME];

  /* unselect all selected items if the user clicks on an empty area
   * of the treeview and no modifier key is active */
  if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0
      && !gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path, &column, NULL, NULL))
    gtk_tree_selection_unselect_all (selection);

  /* make sure that rubber banding is enabled */
  gtk_tree_view_set_rubber_banding (tree_view, TRUE);

  /* if the user clicked on a row with the left button */
  if (path != NULL && event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      GtkTreePath *cursor_path;

      /* find out if the expander was clicked;
       * only needed if expandable folders is enabled */
      if (gtk_tree_view_get_show_expanders (tree_view))
        {
          gtk_tree_view_get_visible_rect (tree_view, &rect);
          gtk_widget_style_get (GTK_WIDGET (tree_view),
                                "expander-size", &expander_size,
                                "horizontal-separator", &horizontal_separator,
                                NULL);
          edge_to_expander_region_width = horizontal_separator / 2 + gtk_tree_path_get_depth (path) * expander_size;
          if (G_LIKELY (gtk_widget_get_direction (GTK_WIDGET (tree_view)) == GTK_TEXT_DIR_LTR))
            on_expander = (event->x <= edge_to_expander_region_width);
          else
            on_expander = (rect.width - event->x <= edge_to_expander_region_width);
        }

      /* grab the tree view */
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_tree_view_get_cursor (tree_view, &cursor_path, NULL);
      if (cursor_path != NULL)
        {
          gtk_tree_path_free (cursor_path);

          /* if single click is enabled and the click was directed
           * towards the expander then expand/collapse the row but do not
           * activate it */
          g_object_get (tree_view, "single-click", &activate_on_single_click, NULL);
          if (on_expander && activate_on_single_click)
            {
              /* select the path and also set the cursor */
              gtk_tree_selection_select_path (selection, path);
              gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

              if (gtk_tree_view_row_expanded (tree_view, path))
                gtk_tree_view_collapse_row (tree_view, path);
              else
                gtk_tree_view_expand_row (tree_view, path, FALSE);

              gtk_tree_path_free (path);
              return TRUE;
            }

          if (column != name_column)
            gtk_tree_selection_unselect_all (selection);

          /* do not start rubber banding from the name column */
          if (!gtk_tree_selection_path_is_selected (selection, path)
              && column == name_column)
            {
              /* the clicked row does not have the focus and is not
               * selected, so make it the new selection (start) */
              gtk_tree_selection_unselect_all (selection);
              if (gtk_tree_model_get_iter (model, &iter, path))
                {
                  file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);
                  if (file != NULL)
                    {
                      row_selected = TRUE;
                      gtk_tree_selection_select_path (selection, path);
                      g_object_unref (file);
                    }
                }

              gtk_tree_path_free (path);

              /* return FALSE to not abort dragging; when row was selected */
              return !row_selected;
            }
          gtk_tree_path_free (path);
          return FALSE;
        }
    }

  /* open the context menu on right clicks */
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      if (path == NULL)
        {
          /* open the context menu */
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view));
        }
      else
        {
          if (column != name_column)
            {
              /* if the clicked path is not selected, unselect all other paths */
              if (!gtk_tree_selection_path_is_selected (selection, path))
                gtk_tree_selection_unselect_all (selection);

              /* queue the menu popup */
              thunar_standard_view_queue_popup (THUNAR_STANDARD_VIEW (details_view), event);
            }
          else
            {
              /* select the clicked path if necessary */
              if (!gtk_tree_selection_path_is_selected (selection, path))
                {
                  gtk_tree_selection_unselect_all (selection);
                  if (gtk_tree_model_get_iter (model, &iter, path))
                    {
                      file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);
                      if (file != NULL)
                        {
                          g_object_unref (file);
                          gtk_tree_selection_select_path (selection, path);
                        }
                    }
                }

              /* queue the menu popup */
              thunar_standard_view_queue_popup (THUNAR_STANDARD_VIEW (details_view), event);
            }
          gtk_tree_path_free (path);
        }

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS && event->button == 2)
    {
      GtkTreePath *mc_path = NULL;

      /* determine the path to the item that was middle-clicked */
      if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &mc_path, NULL, NULL, NULL))
        {
          /* select only the path to the item on which the user clicked */
          gtk_tree_selection_unselect_all (selection);
          if (gtk_tree_model_get_iter (model, &iter, mc_path))
            {
              file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);
              if (file != NULL)
                {
                  g_object_unref (file);
                  gtk_tree_selection_select_path (selection, mc_path);
                  /* try to open the path as new window/tab, if possible */
                  _thunar_standard_view_open_on_middle_click (THUNAR_STANDARD_VIEW (details_view), mc_path, event->state);
                }
            }

          /* cleanup */
          gtk_tree_path_free (mc_path);
        }

      gtk_tree_path_free (path);
      return TRUE;
    }

  gtk_tree_path_free (path);
  return FALSE;
}



static gboolean
tree_view_set_cursor_if_file_not_null (GtkTreeView  *tree_view,
                                       GtkTreeModel *model,
                                       GtkTreePath  *path)
{
  ThunarFile *file;
  GtkTreeIter iter;

  /* sets the cursor if file != NULL for the iter;
   * if cursor set then TRUE is returned; FALSE otherwise */

  _thunar_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), FALSE);
  _thunar_return_val_if_fail (GTK_IS_TREE_MODEL (model), FALSE);
  _thunar_return_val_if_fail (path != NULL, FALSE);

  if (!gtk_tree_model_get_iter (model, &iter, path))
    return FALSE;

  file = thunar_standard_view_model_get_file (THUNAR_STANDARD_VIEW_MODEL (model), &iter);

  if (file == NULL)
    return FALSE;

  g_object_unref (file);
  gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
  return TRUE;
}



static void
thunar_details_view_key_up_set_cursor (GtkTreeView  *tree_view,
                                       GtkTreeModel *model,
                                       GtkTreePath  *cursor)
{
  GtkTreeIter iter;
  gint       *indices;
  gint        depth;

  _thunar_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  _thunar_return_if_fail (GTK_IS_TREE_MODEL (model));
  _thunar_return_if_fail (cursor != NULL);

  if (!gtk_tree_path_prev (cursor))
    {
      /* if we are at the first most toplevel node we have nothing do */
      if (gtk_tree_path_get_depth (cursor) <= 1)
        return;

      /* it might be the first child of an expanded node;
       * in this case we want to point to the parent */
      gtk_tree_path_up (cursor);

      /* the parent path we got now should be perfectly fine;
       * but just in case setting the cursor fails
       * we go and set the prev node of the parent */
      if (G_UNLIKELY (!tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)))
        thunar_details_view_key_up_set_cursor (tree_view, model, cursor);

      return;
    }

  /* set cursor if no child visible */
  if (!gtk_tree_view_row_expanded (tree_view, cursor))
    {
      /* If we are at a row that is loading set cursor will fail;
       * try the previous node */
      if (G_UNLIKELY (!tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)))
        thunar_details_view_key_up_set_cursor (tree_view, model, cursor);
      return;
    }

  /* get the iter for the cursor */
  if (G_UNLIKELY (!gtk_tree_model_get_iter (model, &iter, cursor)))
    return;

  /* set cursor to the last visible child;
   * Works recursively to point to the truly last child */
  gtk_tree_path_down (cursor);
  indices = gtk_tree_path_get_indices_with_depth (cursor, &depth);

  /* point to the last child */
  if (depth > 0)
    indices[depth - 1] = gtk_tree_model_iter_n_children (model, &iter);

  /* If we are at a row that is loading set cursor will fail;
   * try the previous node */
  if (G_UNLIKELY (!tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)))
    thunar_details_view_key_up_set_cursor (tree_view, model, cursor);
}



static void
thunar_details_view_key_down_set_cursor (GtkTreeView  *tree_view,
                                         GtkTreeModel *model,
                                         GtkTreePath  *cursor)
{
  _thunar_return_if_fail (GTK_IS_TREE_VIEW (tree_view));
  _thunar_return_if_fail (GTK_IS_TREE_MODEL (model));
  _thunar_return_if_fail (cursor != NULL);

  if (!gtk_tree_view_row_expanded (tree_view, cursor))
    {
      gtk_tree_path_next (cursor);

      if (tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)
          || gtk_tree_path_get_depth (cursor) <= 1)
        return;

      if (!gtk_tree_path_up (cursor))
        return;

      gtk_tree_path_next (cursor);

      /* If we are at a row that is loading set cursor will fail;
       * try the next node */
      if (G_UNLIKELY (!tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)))
        thunar_details_view_key_down_set_cursor (tree_view, model, cursor);

      return;
    }
  gtk_tree_path_down (cursor);

  /* If we are at a row that is loading set cursor will fail;
   * try the next node */
  if (G_UNLIKELY (!tree_view_set_cursor_if_file_not_null (tree_view, model, cursor)))
    thunar_details_view_key_down_set_cursor (tree_view, model, cursor);
}



static gboolean
thunar_details_view_key_press_event (GtkTreeView       *tree_view,
                                     GdkEventKey       *event,
                                     ThunarDetailsView *details_view)
{
  GtkTreePath  *path;
  gboolean      stopPropagation = FALSE;
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter   iter;
  ThunarFile   *file = NULL;

  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_KEY_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_KEY_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view));
      return TRUE;
    }

  /* don't allow keyboard nav for tree-view if tree-view isn't allowed */
  if (details_view->expandable_folders == FALSE)
    return FALSE;

  /* Get path of currently highlighted item */
  gtk_tree_view_get_cursor (tree_view, &path, NULL);

  if (path == NULL)
    return stopPropagation;

  switch (event->keyval)
    {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
      thunar_details_view_key_up_set_cursor (tree_view, model, path);
      stopPropagation = TRUE;
      break;

    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
      thunar_details_view_key_down_set_cursor (tree_view, model, path);
      stopPropagation = TRUE;
      break;

    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
      /* if branch is expanded then collapse it */
      if (gtk_tree_view_row_expanded (tree_view, path))
        {
          gtk_tree_view_collapse_row (tree_view, path);
        }
      else /* if the branch is already collapsed */
        {
          if (gtk_tree_path_get_depth (path) > 1 && gtk_tree_path_up (path))
            {
              /* if this is not a toplevel item then move to parent */
              gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
            }
        }

      stopPropagation = TRUE;
      break;

    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
      /* if branch is not expanded then expand it */
      if (!gtk_tree_view_row_expanded (tree_view, path))
        {
          gtk_tree_view_expand_row (tree_view, path, FALSE);
        }
      else /* if branch is already expanded then move to first child */
        {
          if (gtk_tree_model_get_iter (model, &iter, path))
            {
              gtk_tree_model_get (model, &iter,
                                  THUNAR_COLUMN_FILE, &file, -1);
              if (file != NULL)
                gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
            }
        }
      stopPropagation = TRUE;
      break;

    case GDK_KEY_space:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
      /* Allow default actions to handle this case */
      GTK_WIDGET_CLASS (thunar_details_view_parent_class)->key_press_event (GTK_WIDGET (tree_view), event);
      stopPropagation = TRUE;
      break;
    }

  gtk_tree_path_free (path);
  if (stopPropagation)
    gtk_widget_grab_focus (GTK_WIDGET (tree_view));

  return stopPropagation;
}



static void
thunar_details_view_row_activated (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column,
                                   ThunarDetailsView *details_view)
{
  ThunarActionManager *action_mgr;
  GtkWidget           *window;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  window = gtk_widget_get_toplevel (GTK_WIDGET (details_view));
  action_mgr = thunar_window_get_action_manager (THUNAR_WINDOW (window));
  thunar_action_manager_activate_selected_files (action_mgr, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY, NULL);
}



static void
thunar_details_view_row_expanded (GtkTreeView       *tree_view,
                                  GtkTreeIter       *parent,
                                  GtkTreePath       *path,
                                  ThunarDetailsView *view)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);

  /* this will replace the dummy child with actual children */
  thunar_tree_view_model_load_subdir (THUNAR_TREE_VIEW_MODEL (model), parent);
}



static void
thunar_details_view_row_collapsed (GtkTreeView       *tree_view,
                                   GtkTreeIter       *iter,
                                   GtkTreePath       *path,
                                   ThunarDetailsView *view)
{
  /* schedule a cleanup with a delay. If the row is expanded before
   * the scheduled cleanup then the cleanup is cancelled. */
  thunar_tree_view_model_schedule_unload (THUNAR_TREE_VIEW_MODEL (THUNAR_STANDARD_VIEW (view)->model), iter);
}



static gboolean
thunar_details_view_select_cursor_row (GtkTreeView       *tree_view,
                                       gboolean           editing,
                                       ThunarDetailsView *details_view)
{
  /* This function is a work-around to fix bug #2487. The default gtk handler for
   * the "select-cursor-row" signal changes the selection to just the cursor row,
   * which prevents multiple file selections being opened. Thus we bypass the gtk
   * signal handler with g_signal_stop_emission_by_name, and emit the "open" action
   * directly. A better long-term solution would be to fix exo to avoid using the
   * default gtk signal handler there.
   */

  ThunarActionManager *action_mgr;
  GtkWidget           *window;

  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (details_view), FALSE);

  g_signal_stop_emission_by_name (tree_view, "select-cursor-row");

  window = gtk_widget_get_toplevel (GTK_WIDGET (details_view));
  action_mgr = thunar_window_get_action_manager (THUNAR_WINDOW (window));
  thunar_action_manager_activate_selected_files (action_mgr, THUNAR_ACTION_MANAGER_CHANGE_DIRECTORY, NULL);

  return TRUE;
}



static void
thunar_details_view_row_changed (GtkTreeView       *tree_view,
                                 GtkTreePath       *path,
                                 GtkTreeViewColumn *column,
                                 ThunarDetailsView *details_view)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  gtk_widget_queue_draw (GTK_WIDGET (details_view));
}



static void
thunar_details_view_columns_changed (ThunarColumnModel *column_model,
                                     ThunarDetailsView *details_view)
{
  ThunarFile         *current_directory;
  const ThunarColumn *column_order;
  ThunarColumn        column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));
  _thunar_return_if_fail (THUNAR_IS_COLUMN_MODEL (column_model));
  _thunar_return_if_fail (details_view->column_model == column_model);

  /* look up current directory */
  current_directory = thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (details_view));

  /* determine the new column order */
  column_order = thunar_column_model_get_column_order (column_model);

  /* apply new order and visibility */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
      gboolean visible = thunar_column_model_get_column_visible (column_model, column);

      /* hide special columns outside of special locations and search mode */
      if (current_directory != NULL)
        {
          if (column == THUNAR_COLUMN_DATE_DELETED)
            visible = thunar_file_is_trash (current_directory);
          else if (column == THUNAR_COLUMN_RECENCY)
            visible = thunar_file_is_recent (current_directory);
          else if (column == THUNAR_COLUMN_LOCATION)
            {
              visible = thunar_file_is_recent (current_directory)
                        || (thunar_standard_view_get_search_query (THUNAR_STANDARD_VIEW (details_view)) != NULL);
            }
        }

      /* apply the new visibility for the tree view column */
      gtk_tree_view_column_set_visible (details_view->columns[column], visible);

      /* change the order of the column relative to its predecessor */
      if (G_LIKELY (column > 0))
        {
          gtk_tree_view_move_column_after (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (details_view))),
                                           details_view->columns[column_order[column]],
                                           details_view->columns[column_order[column - 1]]);
        }
    }
}



static gboolean
thunar_details_view_zoom_level_changed_reload_fixed_height (gpointer data)
{
  ThunarDetailsView *details_view = data;
  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (details_view), FALSE);

  gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (details_view))), TRUE);

  details_view->idle_id = 0;
  return FALSE;
}



static void
thunar_details_view_zoom_level_changed (ThunarDetailsView *details_view)
{
  ThunarColumn column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  if (details_view->fixed_columns == TRUE)
    {
      /* disable fixed_height_mode during resize, otherwise graphical glitches can appear*/
      gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (details_view))), FALSE);
    }

  /* determine the list of tree view columns */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
      /* just queue a resize on this column */
      gtk_tree_view_column_queue_resize (details_view->columns[column]);
    }

  if (details_view->fixed_columns == TRUE)
    {
      /* Call when idle to ensure that gtk_tree_view_column_queue_resize got finished */
      details_view->idle_id = gdk_threads_add_idle (thunar_details_view_zoom_level_changed_reload_fixed_height, details_view);
    }
}



/**
 * thunar_details_view_connect_accelerators:
 * @standard_view : a #ThunarStandardView.
 * @accel_group   : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
static void
thunar_details_view_connect_accelerators (ThunarStandardView *standard_view,
                                          GtkAccelGroup      *accel_group)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  xfce_gtk_accel_map_add_entries (thunar_details_view_action_entries, G_N_ELEMENTS (thunar_details_view_action_entries));
  xfce_gtk_accel_group_connect_action_entries (accel_group,
                                               thunar_details_view_action_entries,
                                               G_N_ELEMENTS (thunar_details_view_action_entries),
                                               standard_view);
}



/**
 * thunar_details_view_disconnect_accelerators:
 * @standard_view : a #ThunarStandardView.
 * @accel_group   : a #GtkAccelGroup to be disconnected
 *
 * Dont listen to the accel keys defined by the action entries any more
 **/
static void
thunar_details_view_disconnect_accelerators (ThunarStandardView *standard_view,
                                             GtkAccelGroup      *accel_group)
{
  /* Dont listen to the accel keys defined by the action entries any more */
  xfce_gtk_accel_group_disconnect_action_entries (accel_group,
                                                  thunar_details_view_action_entries,
                                                  G_N_ELEMENTS (thunar_details_view_action_entries));
}



/**
 * thunar_details_view_append_menu_items:
 * @standard_view : a #ThunarStandardView.
 * @menu          : the #GtkMenu to add the menu items.
 * @accel_group   : a #GtkAccelGroup to be used used for new menu items
 *
 * Appends widget-specific menu items to a #GtkMenu and connects them to the passed #GtkAccelGroup
 * Implements method 'append_menu_items' of #ThunarStandardView
 **/
static void
thunar_details_view_append_menu_items (ThunarStandardView *standard_view,
                                       GtkMenu            *menu,
                                       GtkAccelGroup      *accel_group)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  xfce_gtk_menu_item_new_from_action_entry (get_action_entry (THUNAR_DETAILS_VIEW_ACTION_CONFIGURE_COLUMNS), G_OBJECT (details_view), GTK_MENU_SHELL (menu));
  xfce_gtk_toggle_menu_item_new_from_action_entry (get_action_entry (THUNAR_DETAILS_VIEW_ACTION_TOGGLE_EXPANDABLE_FOLDERS), G_OBJECT (details_view), details_view->expandable_folders, GTK_MENU_SHELL (menu));
}



/**
 * thunar_details_view_get_fixed_columns:
 * @details_view : a #ThunarDetailsView.
 *
 * Returns %TRUE if @details_view uses fixed column widths.
 *
 * Return value: %TRUE if @details_view uses fixed column widths.
 **/
static gboolean
thunar_details_view_get_fixed_columns (ThunarDetailsView *details_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (details_view), FALSE);
  return details_view->fixed_columns;
}



/**
 * thunar_details_view_set_fixed_columns:
 * @details_view  : a #ThunarDetailsView.
 * @fixed_columns : %TRUE to use fixed column widths.
 *
 * Applies @fixed_columns to @details_view.
 **/
static void
thunar_details_view_set_fixed_columns (ThunarDetailsView *details_view,
                                       gboolean           fixed_columns)
{
  ThunarColumn column;
  gint         width;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  /* normalize the value */
  fixed_columns = !!fixed_columns;

  /* check if we have a new value */
  if (G_LIKELY (details_view->fixed_columns != fixed_columns))
    {
      /* apply the new value */
      details_view->fixed_columns = fixed_columns;

      /* disable in reverse order, otherwise graphical glitches can appear*/
      if (!fixed_columns)
        gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (details_view))), FALSE);

      /* apply the new setting to all columns */
      for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
        {
          /* apply the new column mode */
          if (G_LIKELY (fixed_columns))
            {
              /* apply "width" as "fixed-width" for fixed columns mode */
              width = gtk_tree_view_column_get_width (details_view->columns[column]);
              if (G_UNLIKELY (width <= 0))
                width = thunar_column_model_get_column_width (details_view->column_model, column);
              gtk_tree_view_column_set_fixed_width (details_view->columns[column], MAX (width, 1));

              /* set column to fixed width */
              gtk_tree_view_column_set_sizing (details_view->columns[column], GTK_TREE_VIEW_COLUMN_FIXED);
            }
          else
            {
              /* reset column to grow-only mode */
              gtk_tree_view_column_set_sizing (details_view->columns[column], GTK_TREE_VIEW_COLUMN_GROW_ONLY);
              gtk_tree_view_column_set_fixed_width (details_view->columns[column], -1);
            }
        }

      /* for fixed columns mode, we can enable the fixed height
       * mode to improve the performance of the GtkTreeVeiw. */
      if (fixed_columns)
        gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (gtk_bin_get_child (GTK_BIN (details_view))), TRUE);

      if (fixed_columns == FALSE)
        {
          /* Make sure the name column auto-expands */
          gtk_tree_view_column_set_expand (details_view->columns[THUNAR_COLUMN_NAME], TRUE);
        }

      gtk_tree_view_column_queue_resize (details_view->columns[THUNAR_COLUMN_NAME]);

      /* notify listeners */
      g_object_notify (G_OBJECT (details_view), "fixed-columns");
    }
}



/**
 * thunar_details_view_set_expandable_folders:
 * @details_view  : a #ThunarDetailsView.
 * @expandable_folders : %TRUE to enable expansion of folders.
 *
 * Sets @expandable_folders in @details_view.
 * Accordingly, updates view to show expanders and tree-lines.
 **/
static void
thunar_details_view_set_expandable_folders (ThunarDetailsView *details_view,
                                            gboolean           expandable_folders)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  details_view->expandable_folders = expandable_folders;

  gtk_tree_view_set_show_expanders (GTK_TREE_VIEW (details_view->tree_view), details_view->expandable_folders);
  gtk_tree_view_set_enable_tree_lines (GTK_TREE_VIEW (details_view->tree_view), details_view->expandable_folders);

  if (details_view->expandable_folders == FALSE)
    gtk_tree_view_collapse_all (GTK_TREE_VIEW (details_view->tree_view));

  /* notify listeners */
  g_object_notify (G_OBJECT (details_view), "expandable-folders");
}



/**
 * thunar_details_view_set_date_deleted_column_visible:
 * @details_view  : a #ThunarDetailsView.
 * @visible : %TRUE to show the Date Deleted column.
 *
 * Shows/hides the Date Deleted column.
 **/
void
thunar_details_view_set_date_deleted_column_visible (ThunarDetailsView *details_view,
                                                     gboolean           visible)
{
  thunar_column_model_set_column_visible (details_view->column_model, THUNAR_COLUMN_DATE_DELETED, visible);
}



void
thunar_details_view_set_recency_column_visible (ThunarDetailsView *details_view,
                                                gboolean           visible)
{
  thunar_column_model_set_column_visible (details_view->column_model, THUNAR_COLUMN_RECENCY, visible);
}



void
thunar_details_view_set_location_column_visible (ThunarDetailsView *details_view,
                                                 gboolean           visible)
{
  thunar_column_model_set_column_visible (details_view->column_model, THUNAR_COLUMN_LOCATION, visible);
}



static void
thunar_details_view_highlight_option_changed (ThunarDetailsView *details_view)
{
  GtkTreeCellDataFunc function = NULL;
  gboolean            show_highlight;
  gint                column;

  g_object_get (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences), "misc-highlighting-enabled", &show_highlight, NULL);

  if (show_highlight)
    function = (GtkTreeCellDataFunc) THUNAR_STANDARD_VIEW_GET_CLASS (details_view)->cell_layout_data_func;

  /* set the data functions for the respective renderers */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; column++)
    {
      if (column == THUNAR_COLUMN_NAME)
        continue;
      gtk_tree_view_column_set_cell_data_func (GTK_TREE_VIEW_COLUMN (details_view->columns[column]),
                                               GTK_CELL_RENDERER (details_view->renderers[column]),
                                               function, THUNAR_STANDARD_VIEW (details_view), NULL);
    }
}



static void
thunar_details_view_queue_redraw (ThunarStandardView *standard_view)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (standard_view);

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  gtk_widget_queue_draw (GTK_WIDGET (details_view->tree_view));
}



/**
 * thunar_details_view_toggle_expandable_folders:
 * @details_view  : a #ThunarDetailsView.
 *
 * Toggles the expandable folders feature.
 **/
static gboolean
thunar_details_view_toggle_expandable_folders (ThunarDetailsView *details_view)
{
  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (details_view), FALSE);

  thunar_details_view_set_expandable_folders (details_view, !details_view->expandable_folders);

  return TRUE;
}



static void
thunar_details_view_block_selection_changed (ThunarStandardView *view)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (view);
  GtkTreeSelection  *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (details_view->tree_view));
  g_signal_handlers_block_by_func (G_OBJECT (selection), thunar_standard_view_selection_changed, view);
}



static void
thunar_details_view_unblock_selection_changed (ThunarStandardView *view)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (view);
  GtkTreeSelection  *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (details_view->tree_view));
  g_signal_handlers_unblock_by_func (G_OBJECT (selection), thunar_standard_view_selection_changed, view);
}
