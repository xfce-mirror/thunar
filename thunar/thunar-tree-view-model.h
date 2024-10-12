/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2023 Amrit Borah <elessar1802@gmail.com>
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

#ifndef __THUNAR_TREE_VIEW_MODEL_H__
#define __THUNAR_TREE_VIEW_MODEL_H__

#include "thunar/thunar-standard-view-model.h"

G_BEGIN_DECLS;

typedef struct _ThunarTreeViewModelClass ThunarTreeViewModelClass;
typedef struct _ThunarTreeViewModel      ThunarTreeViewModel;

#define THUNAR_TYPE_TREE_VIEW_MODEL (thunar_tree_view_model_get_type ())
#define THUNAR_TREE_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModel))
#define THUNAR_TREE_VIEW_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModelClass))
#define THUNAR_IS_TREE_VIEW_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TREE_VIEW_MODEL))
#define THUNAR_IS_TREE_VIEW_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TREE_VIEW_MODEL))
#define THUNAR_TREE_VIEW_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TREE_VIEW_MODEL, ThunarTreeViewModelClass))

GType
thunar_tree_view_model_get_type (void) G_GNUC_CONST;

ThunarStandardViewModel *
thunar_tree_view_model_new (void);
void
thunar_tree_view_model_load_subdir (ThunarTreeViewModel *model,
                                    GtkTreeIter         *iter);
void
thunar_tree_view_model_schedule_unload (ThunarTreeViewModel *model,
                                        GtkTreeIter         *iter);
G_END_DECLS;

#endif /* !__THUNAR_TREE_VIEW_MODEL_H__ */
