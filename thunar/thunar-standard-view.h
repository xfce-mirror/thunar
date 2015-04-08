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

#ifndef __THUNAR_STANDARD_VIEW_H__
#define __THUNAR_STANDARD_VIEW_H__

#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-history.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-list-model.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-view.h>

G_BEGIN_DECLS;

typedef struct _ThunarStandardViewPrivate ThunarStandardViewPrivate;
typedef struct _ThunarStandardViewClass   ThunarStandardViewClass;
typedef struct _ThunarStandardView        ThunarStandardView;

#define THUNAR_TYPE_STANDARD_VIEW             (thunar_standard_view_get_type ())
#define THUNAR_STANDARD_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardView))
#define THUNAR_STANDARD_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewClass))
#define THUNAR_IS_STANDARD_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_STANDARD_VIEW))
#define THUNAR_IS_STANDARD_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_STANDARD_VIEW))
#define THUNAR_STANDARD_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_STANDARD_VIEW, ThunarStandardViewClass))

struct _ThunarStandardViewClass
{
  GtkScrolledWindowClass __parent__;

  /* Called by the ThunarStandardView class to let derived classes
   * connect to and disconnect from the UI manager.
   */
  void       (*connect_ui_manager)      (ThunarStandardView *standard_view,
                                         GtkUIManager       *ui_manager);
  void       (*disconnect_ui_manager)   (ThunarStandardView *standard_view,
                                         GtkUIManager       *ui_manager);

  /* Returns the list of currently selected GtkTreePath's, where
   * both the list and the items are owned by the caller. */
  GList       *(*get_selected_items)    (ThunarStandardView *standard_view);

  /* Selects all items in the view */
  void         (*select_all)            (ThunarStandardView *standard_view);

  /* Unselects all items in the view */
  void         (*unselect_all)          (ThunarStandardView *standard_view);

  /* Invert selection in the view */
  void         (*selection_invert)      (ThunarStandardView *standard_view);

  /* Selects the given item */
  void         (*select_path)           (ThunarStandardView *standard_view,
                                         GtkTreePath        *path);

  /* Called by the ThunarStandardView class to let derived class
   * place the cursor on the item/row referred to by path. If
   * start_editing is TRUE, the derived class should also start
   * editing that item/row.
   */
  void         (*set_cursor)            (ThunarStandardView *standard_view,
                                         GtkTreePath        *path,
                                         gboolean            start_editing);

  /* Called by the ThunarStandardView class to let derived class
   * scroll the view to the given path.
   */
  void         (*scroll_to_path)        (ThunarStandardView *standard_view,
                                         GtkTreePath        *path,
                                         gboolean            use_align,
                                         gfloat              row_align,
                                         gfloat              col_align);

  /* Returns the path at the given position or NULL if no item/row
   * is located at that coordinates. The path is freed by the caller.
   */
  GtkTreePath *(*get_path_at_pos)       (ThunarStandardView *standard_view,
                                         gint                x,
                                         gint                y);

  /* Returns the visible range */
  gboolean     (*get_visible_range)     (ThunarStandardView *standard_view,
                                         GtkTreePath       **start_path,
                                         GtkTreePath       **end_path);

  /* Sets the item/row that is highlighted for feedback. NULL is
   * passed for path to disable the highlighting.
   */
  void         (*highlight_path)        (ThunarStandardView  *standard_view,
                                         GtkTreePath         *path);

  /* external signals */
  void         (*start_open_location)   (ThunarStandardView *standard_view,
                                         const gchar        *initial_text);

  /* Internal action signals */
  gboolean     (*delete_selected_files) (ThunarStandardView *standard_view);

  /* The name of the property in ThunarPreferences, that determines
   * the last (and default) zoom-level for the view classes (i.e. in
   * case of ThunarIconView, this is "last-icon-view-zoom-level").
   */
  const gchar *zoom_level_property_name;
};

struct _ThunarStandardView
{
  GtkScrolledWindow __parent__;

  ThunarPreferences         *preferences;

  ThunarClipboardManager    *clipboard;
  ThunarListModel           *model;

  GtkActionGroup            *action_group;
  GtkUIManager              *ui_manager;
  guint                      ui_merge_id;

  ThunarIconFactory         *icon_factory;
  GtkCellRenderer           *icon_renderer;
  GtkCellRenderer           *name_renderer;

  ExoBinding                *loading_binding;
  gboolean                   loading;

  ThunarStandardViewPrivate *priv;
};

GType thunar_standard_view_get_type           (void) G_GNUC_CONST;

void  thunar_standard_view_context_menu       (ThunarStandardView *standard_view,
                                               guint               button,
                                               guint32             time);

void  thunar_standard_view_queue_popup        (ThunarStandardView *standard_view,
                                               GdkEventButton     *event);

void  thunar_standard_view_selection_changed  (ThunarStandardView *standard_view);


void  thunar_standard_view_set_history            (ThunarStandardView *standard_view,
                                                   ThunarHistory      *history);

ThunarHistory *thunar_standard_view_copy_history  (ThunarStandardView *standard_view);

G_END_DECLS;

#endif /* !__THUNAR_STANDARD_VIEW_H__ */
