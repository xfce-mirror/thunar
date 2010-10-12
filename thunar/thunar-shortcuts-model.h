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

#ifndef __THUNAR_SHORTCUTS_MODEL_H__
#define __THUNAR_SHORTCUTS_MODEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarShortcutsModelClass ThunarShortcutsModelClass;
typedef struct _ThunarShortcutsModel      ThunarShortcutsModel;

#define THUNAR_TYPE_SHORTCUTS_MODEL            (thunar_shortcuts_model_get_type ())
#define THUNAR_SHORTCUTS_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SHORTCUTS_MODEL, ThunarShortcutsModel))
#define THUNAR_SHORTCUTS_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SHORTCUTS_MODEL, ThunarShortcutsModelClass))
#define THUNAR_IS_SHORTCUTS_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SHORTCUTS_MODEL))
#define THUNAR_IS_SHORTCUTS_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SHORTCUTS_MODEL))
#define THUNAR_SHORTCUTS_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MODEL_SHORTCUTS_MODEL, ThunarShortcutsModelClass))

/**
 * ThunarShortcutsModelColumn:
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_NAME      : the index of the name column.
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_FILE      : the index of the file column.
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME    : the index of the volume column.
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE   : tells whether a row is mutable.
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT     : stock icon name for eject symbol
 * @THUNAR_SHORTCUTS_MODEL_COLUMN_SEPARATOR : tells whether a row is a separator.
 *
 * Columns exported by #ThunarShortcutsModel using the
 * #GtkTreeModel interface.
 **/
typedef enum
{
  THUNAR_SHORTCUTS_MODEL_COLUMN_NAME,
  THUNAR_SHORTCUTS_MODEL_COLUMN_FILE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_VOLUME,
  THUNAR_SHORTCUTS_MODEL_COLUMN_MUTABLE,
  THUNAR_SHORTCUTS_MODEL_COLUMN_EJECT,
  THUNAR_SHORTCUTS_MODEL_COLUMN_SEPARATOR,
  THUNAR_SHORTCUTS_MODEL_N_COLUMNS,
} ThunarShortcutsModelColumn;

GType                  thunar_shortcuts_model_get_type      (void) G_GNUC_CONST;

ThunarShortcutsModel *thunar_shortcuts_model_get_default    (void);

gboolean               thunar_shortcuts_model_iter_for_file (ThunarShortcutsModel *model,
                                                             ThunarFile           *file,
                                                             GtkTreeIter          *iter);

gboolean               thunar_shortcuts_model_drop_possible (ThunarShortcutsModel *model,
                                                             GtkTreePath          *path);

void                   thunar_shortcuts_model_add           (ThunarShortcutsModel *model,
                                                             GtkTreePath          *dst_path,
                                                             ThunarFile           *file);
void                   thunar_shortcuts_model_move          (ThunarShortcutsModel *model,
                                                             GtkTreePath          *src_path,
                                                             GtkTreePath          *dst_path);
void                   thunar_shortcuts_model_remove        (ThunarShortcutsModel *model,
                                                             GtkTreePath          *path);
void                   thunar_shortcuts_model_rename        (ThunarShortcutsModel *model,
                                                             GtkTreeIter          *iter,
                                                             const gchar          *name);

G_END_DECLS;

#endif /* !__THUNAR_SHORTCUTS_MODEL_H__ */
