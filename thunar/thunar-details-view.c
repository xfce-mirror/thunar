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
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-column-editor.h>
#include <thunar/thunar-details-view.h>
#include <thunar/thunar-details-view-ui.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-preferences.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FIXED_COLUMNS,
};



static void         thunar_details_view_finalize                (GObject                *object);
static void         thunar_details_view_get_property            (GObject                *object,
                                                                 guint                   prop_id,
                                                                 GValue                 *value,
                                                                 GParamSpec             *pspec);
static void         thunar_details_view_set_property            (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
static AtkObject   *thunar_details_view_get_accessible          (GtkWidget              *widget);
static void         thunar_details_view_connect_ui_manager      (ThunarStandardView     *standard_view,
                                                                 GtkUIManager           *ui_manager);
static void         thunar_details_view_disconnect_ui_manager   (ThunarStandardView     *standard_view,
                                                                 GtkUIManager           *ui_manager);
static GList       *thunar_details_view_get_selected_items      (ThunarStandardView     *standard_view);
static void         thunar_details_view_select_all              (ThunarStandardView     *standard_view);
static void         thunar_details_view_unselect_all            (ThunarStandardView     *standard_view);
static void         thunar_details_view_selection_invert        (ThunarStandardView     *standard_view);
static void         thunar_details_view_select_path             (ThunarStandardView     *standard_view,
                                                                 GtkTreePath            *path);
static void         thunar_details_view_set_cursor              (ThunarStandardView     *standard_view,
                                                                 GtkTreePath            *path,
                                                                 gboolean                start_editing);
static void         thunar_details_view_scroll_to_path          (ThunarStandardView     *standard_view,
                                                                 GtkTreePath            *path,
                                                                 gboolean                use_align,
                                                                 gfloat                  row_align,
                                                                 gfloat                  col_align);
static GtkTreePath *thunar_details_view_get_path_at_pos         (ThunarStandardView     *standard_view,
                                                                 gint                    x,
                                                                 gint                    y);
static gboolean     thunar_details_view_get_visible_range       (ThunarStandardView     *standard_view,
                                                                 GtkTreePath           **start_path,
                                                                 GtkTreePath           **end_path);
static void         thunar_details_view_highlight_path          (ThunarStandardView     *standard_view,
                                                                 GtkTreePath            *path);
static void         thunar_details_view_notify_model            (GtkTreeView            *tree_view,
                                                                 GParamSpec             *pspec,
                                                                 ThunarDetailsView      *details_view);
static void         thunar_details_view_notify_width            (GtkTreeViewColumn      *tree_view_column,
                                                                 GParamSpec             *pspec,
                                                                 ThunarDetailsView      *details_view);
static gboolean     thunar_details_view_button_press_event      (GtkTreeView            *tree_view,
                                                                 GdkEventButton         *event,
                                                                 ThunarDetailsView      *details_view);
static gboolean     thunar_details_view_key_press_event         (GtkTreeView            *tree_view,
                                                                 GdkEventKey            *event,
                                                                 ThunarDetailsView      *details_view);
static void         thunar_details_view_row_activated           (GtkTreeView            *tree_view,
                                                                 GtkTreePath            *path,
                                                                 GtkTreeViewColumn      *column,
                                                                 ThunarDetailsView      *details_view);
static void         thunar_details_view_row_changed             (GtkTreeView            *tree_view,
                                                                 GtkTreePath            *path,
                                                                 GtkTreeViewColumn      *column,
                                                                 ThunarDetailsView      *details_view);
static void         thunar_details_view_columns_changed         (ThunarColumnModel      *column_model,
                                                                 ThunarDetailsView      *details_view);
static void         thunar_details_view_zoom_level_changed      (ThunarDetailsView      *details_view);
static void         thunar_details_view_action_setup_columns    (GtkAction              *action,
                                                                 ThunarDetailsView      *details_view);
static gboolean     thunar_details_view_get_fixed_columns       (ThunarDetailsView      *details_view);
static void         thunar_details_view_set_fixed_columns       (ThunarDetailsView      *details_view,
                                                                 gboolean                fixed_columns);



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
  gboolean           fixed_columns;

  /* the UI manager merge id for the details view */
  guint              ui_merge_id;
};



static const GtkActionEntry action_entries[] =
{
  { "setup-columns", NULL, N_ ("Configure _Columns..."), NULL, N_ ("Configure the columns in the detailed list view"), G_CALLBACK (thunar_details_view_action_setup_columns), },
};



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
  thunarstandard_view_class->connect_ui_manager = thunar_details_view_connect_ui_manager;
  thunarstandard_view_class->disconnect_ui_manager = thunar_details_view_disconnect_ui_manager;
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
  thunarstandard_view_class->zoom_level_property_name = "last-details-view-zoom-level";

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
}



static void
thunar_details_view_init (ThunarDetailsView *details_view)
{
  GtkTreeSelection *selection;
  GtkCellRenderer  *right_aligned_renderer;
  GtkCellRenderer  *left_aligned_renderer;
  GtkCellRenderer  *renderer;
  ThunarColumn      column;
  GtkWidget        *tree_view;

  /* we need to force the GtkTreeView to recalculate column sizes
   * whenever the zoom-level changes, so we connect a handler here.
   */
  g_signal_connect (G_OBJECT (details_view), "notify::zoom-level", G_CALLBACK (thunar_details_view_zoom_level_changed), NULL);

  /* setup the details view actions */
  gtk_action_group_add_actions (THUNAR_STANDARD_VIEW (details_view)->action_group,
                                action_entries, G_N_ELEMENTS (action_entries),
                                GTK_WIDGET (details_view));

  /* create the tree view to embed */
  tree_view = exo_tree_view_new ();
  g_signal_connect (G_OBJECT (tree_view), "notify::model",
                    G_CALLBACK (thunar_details_view_notify_model), details_view);
  g_signal_connect (G_OBJECT (tree_view), "button-press-event",
                    G_CALLBACK (thunar_details_view_button_press_event), details_view);
  g_signal_connect (G_OBJECT (tree_view), "key-press-event",
                    G_CALLBACK (thunar_details_view_key_press_event), details_view);
  g_signal_connect (G_OBJECT (tree_view), "row-activated",
                    G_CALLBACK (thunar_details_view_row_activated), details_view);
  gtk_container_add (GTK_CONTAINER (details_view), tree_view);
  gtk_widget_show (tree_view);

  /* configure general aspects of the details view */
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree_view), TRUE);
  gtk_tree_view_set_enable_search (GTK_TREE_VIEW (tree_view), TRUE);

  /* enable rubberbanding (if supported) */
  gtk_tree_view_set_rubber_banding (GTK_TREE_VIEW (tree_view), TRUE);

  /* connect to the default column model */
  details_view->column_model = thunar_column_model_get_default ();
  g_signal_connect (G_OBJECT (details_view->column_model), "columns-changed", G_CALLBACK (thunar_details_view_columns_changed), details_view);
  g_signal_connect_after (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->model), "row-changed",
                          G_CALLBACK (thunar_details_view_row_changed), details_view);

  /* allocate the shared right-aligned text renderer */
  right_aligned_renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER, "xalign", 1.0f, NULL);
  g_object_ref_sink (G_OBJECT (right_aligned_renderer));

  /* allocate the shared left-aligned text renderer */
  left_aligned_renderer = g_object_new (THUNAR_TYPE_TEXT_RENDERER, "xalign", 0.0f, NULL);
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
          g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->name_renderer), "xalign", 0.0, NULL);
          gtk_tree_view_column_pack_start (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->name_renderer, TRUE);
          gtk_tree_view_column_set_attributes (details_view->columns[column], THUNAR_STANDARD_VIEW (details_view)->name_renderer,
                                               "text", THUNAR_COLUMN_NAME,
                                               NULL);

          /* add some spacing between the icon and the name */
          gtk_tree_view_column_set_spacing (details_view->columns[column], 2);
          gtk_tree_view_column_set_expand (details_view->columns[column], TRUE);
        }
      else
        {
          /* size is right aligned, everything else is left aligned */
          renderer = (column == THUNAR_COLUMN_SIZE) ? right_aligned_renderer : left_aligned_renderer;

          /* add the renderer */
          gtk_tree_view_column_pack_start (details_view->columns[column], renderer, TRUE);
          gtk_tree_view_column_set_attributes (details_view->columns[column], renderer, "text", column, NULL);
        }

      /* append the tree view column to the tree view */
      gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), details_view->columns[column]);
    }

  /* configure the tree selection */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect_swapped (G_OBJECT (selection), "changed",
                            G_CALLBACK (thunar_standard_view_selection_changed), details_view);

  /* apply the initial column order and visibility from the column model */
  thunar_details_view_columns_changed (details_view->column_model, details_view);

  /* synchronize the "fixed-columns" property */
  exo_binding_new (G_OBJECT (THUNAR_STANDARD_VIEW (details_view)->preferences),
                   "last-details-view-fixed-columns",
                   G_OBJECT (details_view),
                   "fixed-columns");

  /* apply fixed column widths if we start in fixed column mode */
  if (G_UNLIKELY (details_view->fixed_columns))
    {
      /* apply to all columns */
      for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
        gtk_tree_view_column_set_fixed_width (details_view->columns[column], thunar_column_model_get_column_width (details_view->column_model, column));
    }

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

  (*G_OBJECT_CLASS (thunar_details_view_parent_class)->finalize) (object);
}



static AtkObject*
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



static void
thunar_details_view_connect_ui_manager (ThunarStandardView *standard_view,
                                        GtkUIManager       *ui_manager)
{
  ThunarDetailsView *details_view = THUNAR_DETAILS_VIEW (standard_view);
  GError            *error = NULL;

  details_view->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_details_view_ui,
                                                                 thunar_details_view_ui_length, &error);
  if (G_UNLIKELY (details_view->ui_merge_id == 0))
    {
      g_error ("Failed to merge ThunarDetailsView menus: %s", error->message);
      g_error_free (error);
    }
}



static void
thunar_details_view_disconnect_ui_manager (ThunarStandardView *standard_view,
                                           GtkUIManager       *ui_manager)
{
  gtk_ui_manager_remove_ui (ui_manager, THUNAR_DETAILS_VIEW (standard_view)->ui_merge_id);
}



static GList*
thunar_details_view_get_selected_items (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  return gtk_tree_selection_get_selected_rows (selection, NULL);
}



static void
thunar_details_view_select_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  gtk_tree_selection_select_all (selection);
}



static void
thunar_details_view_unselect_all (ThunarStandardView *standard_view)
{
  GtkTreeSelection *selection;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
  gtk_tree_selection_unselect_all (selection);
}



static void
thunar_details_view_selection_invert_foreach (GtkTreeModel *model,
                                              GtkTreePath  *path,
                                              GtkTreeIter  *iter,
                                              gpointer      data)
{
  GList      **list = data;

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

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));

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

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (GTK_BIN (standard_view)->child));
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
  mode = standard_view->name_renderer->mode;
  standard_view->name_renderer->mode = GTK_CELL_RENDERER_MODE_EDITABLE;

  /* tell the tree view to start editing the given row */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), 0);
  gtk_tree_view_set_cursor_on_cell (GTK_TREE_VIEW (GTK_BIN (standard_view)->child),
                                    path, column, standard_view->name_renderer,
                                    start_editing);

  /* reset the name renderer mode */
  standard_view->name_renderer->mode = mode;
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

  /* tell the tree view to scroll to the given row */
  column = gtk_tree_view_get_column (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), 0);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), path, column, use_align, row_align, col_align);
}



static GtkTreePath*
thunar_details_view_get_path_at_pos (ThunarStandardView *standard_view,
                                     gint                x,
                                     gint                y)
{
  GtkTreePath *path;

  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), NULL);

  if (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), x, y, &path, NULL))
    return path;

  return NULL;
}



static gboolean
thunar_details_view_get_visible_range (ThunarStandardView *standard_view,
                                       GtkTreePath       **start_path,
                                       GtkTreePath       **end_path)
{
  _thunar_return_val_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view), FALSE);

  return gtk_tree_view_get_visible_range (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), start_path, end_path);
}



static void
thunar_details_view_highlight_path (ThunarStandardView *standard_view,
                                    GtkTreePath        *path)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (standard_view));
  gtk_tree_view_set_drag_dest_row (GTK_TREE_VIEW (GTK_BIN (standard_view)->child), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
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
thunar_details_view_button_press_event (GtkTreeView       *tree_view,
                                        GdkEventButton    *event,
                                        ThunarDetailsView *details_view)
{
  GtkTreeSelection  *selection;
  GtkTreePath       *path = NULL;
  GtkTreeIter        iter;
  GtkTreeViewColumn *column;
  GtkTreeViewColumn *name_column;
  ThunarFile        *file;
  GtkAction         *action;
  ThunarPreferences *preferences;
  gboolean           in_tab;
  const gchar       *action_name;

  /* check if the event is for the bin window */
  if (G_UNLIKELY (event->window != gtk_tree_view_get_bin_window (tree_view)))
    return FALSE;

  /* get the current selection */
  selection = gtk_tree_view_get_selection (tree_view);

  /* get the column showing the filenames */
  name_column = details_view->columns[THUNAR_COLUMN_NAME];

  /* unselect all selected items if the user clicks on an empty area
   * of the treeview and no modifier key is active */
  if ((event->state & gtk_accelerator_get_default_mod_mask ()) == 0
      && !gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path, &column, NULL, NULL))
      gtk_tree_selection_unselect_all (selection);

  /* if the user clicked on a row with the left button */
  if (path != NULL && event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
      GtkTreePath       *cursor_path;

      /* grab the tree view */
      gtk_widget_grab_focus (GTK_WIDGET (tree_view));

      gtk_tree_view_get_cursor (tree_view, &cursor_path, NULL);
      if (cursor_path != NULL)
        {
          gtk_tree_path_free (cursor_path);

          if (column != name_column)
            gtk_tree_selection_unselect_all (selection);

          /* do not start rubber banding from the name column */
          if (!gtk_tree_selection_path_is_selected (selection, path)
              && column == name_column)
            {
              /* the clicked row does not have the focus and is not
               * selected, so make it the new selection (start) */
              gtk_tree_selection_unselect_all (selection);
              gtk_tree_selection_select_path (selection, path);

              gtk_tree_path_free (path);

              /* return FALSE to not abort dragging */
              return FALSE;
            }
          gtk_tree_path_free (path);
        }
    }

  /* open the context menu on right clicks */
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
      if (path == NULL)
        {
          /* open the context menu */
          thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view), event->button, event->time);
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
                  gtk_tree_selection_select_path (selection, path);
                }

              /* show the context menu */
              thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view), event->button, event->time);
            }
          gtk_tree_path_free (path);
        }

      return TRUE;
    }
  else if (event->type == GDK_BUTTON_PRESS && event->button == 2)
    {
      /* determine the path to the item that was middle-clicked */
      if (gtk_tree_view_get_path_at_pos (tree_view, event->x, event->y, &path, NULL, NULL, NULL))
        {
          /* select only the path to the item on which the user clicked */
          gtk_tree_selection_unselect_all (selection);
          gtk_tree_selection_select_path (selection, path);

          /* determine the file for the path */
          gtk_tree_model_get_iter (GTK_TREE_MODEL (THUNAR_STANDARD_VIEW (details_view)->model), &iter, path);
          file = thunar_list_model_get_file (THUNAR_STANDARD_VIEW (details_view)->model, &iter);
          if (G_LIKELY (file != NULL) && thunar_file_is_directory (file))
            {
              /* lookup setting if we should open in a tab or a window */
              preferences = thunar_preferences_get ();
              g_object_get (preferences, "misc-middle-click-in-tab", &in_tab, NULL);
              g_object_unref (preferences);

              /* holding ctrl inverts the action */
              if ((event->state & GDK_CONTROL_MASK) != 0)
                  in_tab = !in_tab;

              action_name = in_tab ? "open-in-new-tab" : "open-in-new-window";

              action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (details_view)->ui_manager, action_name);

              /* emit the action */
              if (G_LIKELY (action != NULL))
                  gtk_action_activate (action);

              /* release the file reference */
              g_object_unref (G_OBJECT (file));
            }

          /* cleanup */
          gtk_tree_path_free (path);
        }

      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_details_view_key_press_event (GtkTreeView       *tree_view,
                                     GdkEventKey       *event,
                                     ThunarDetailsView *details_view)
{
  /* popup context menu if "Menu" or "<Shift>F10" is pressed */
  if (event->keyval == GDK_Menu || ((event->state & GDK_SHIFT_MASK) != 0 && event->keyval == GDK_F10))
    {
      thunar_standard_view_context_menu (THUNAR_STANDARD_VIEW (details_view), 0, event->time);
      return TRUE;
    }

  return FALSE;
}



static void
thunar_details_view_row_activated (GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column,
                                   ThunarDetailsView *details_view)
{
  GtkTreeSelection *selection;
  GtkAction        *action;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  /* be sure to have only the double clicked item selected */
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_unselect_all (selection);
  gtk_tree_selection_select_path (selection, path);

  /* emit the "open" action */
  action = thunar_gtk_ui_manager_get_action_by_name (THUNAR_STANDARD_VIEW (details_view)->ui_manager, "open");
  if (G_LIKELY (action != NULL))
    gtk_action_activate (action);
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
  const ThunarColumn *column_order;
  ThunarColumn        column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));
  _thunar_return_if_fail (THUNAR_IS_COLUMN_MODEL (column_model));
  _thunar_return_if_fail (details_view->column_model == column_model);

  /* determine the new column order */
  column_order = thunar_column_model_get_column_order (column_model);

  /* apply new order and visibility */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
      /* apply the new visibility for the tree view column */
      gtk_tree_view_column_set_visible (details_view->columns[column], thunar_column_model_get_column_visible (column_model, column));

      /* change the order of the column relative to its predecessor */
      if (G_LIKELY (column > 0))
        {
          gtk_tree_view_move_column_after (GTK_TREE_VIEW (GTK_BIN (details_view)->child),
                                           details_view->columns[column_order[column]],
                                           details_view->columns[column_order[column - 1]]);
        }
    }
}



static void
thunar_details_view_zoom_level_changed (ThunarDetailsView *details_view)
{
  ThunarColumn column;

  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));

  /* determine the list of tree view columns */
  for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
      /* just queue a resize on this column */
      gtk_tree_view_column_queue_resize (details_view->columns[column]);
    }
}



static void
thunar_details_view_action_setup_columns (GtkAction         *action,
                                          ThunarDetailsView *details_view)
{
  _thunar_return_if_fail (THUNAR_IS_DETAILS_VIEW (details_view));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* popup the column editor dialog */
  thunar_show_column_editor (details_view);
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
            }
        }

      /* for fixed columns mode, we can enable the fixed height
       * mode to improve the performance of the GtkTreeVeiw.
       */
      gtk_tree_view_set_fixed_height_mode (GTK_TREE_VIEW (GTK_BIN (details_view)->child), fixed_columns);

      /* notify listeners */
      g_object_notify (G_OBJECT (details_view), "fixed-columns");
    }
}
