/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_LIST_MODEL_H__
#define __THUNAR_LIST_MODEL_H__

#include "thunar/thunar-standard-view-model.h"

G_BEGIN_DECLS;

typedef struct _ThunarListModelClass ThunarListModelClass;
typedef struct _ThunarListModel      ThunarListModel;

#define THUNAR_TYPE_LIST_MODEL (thunar_list_model_get_type ())
#define THUNAR_LIST_MODEL(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_LIST_MODEL, ThunarListModel))
#define THUNAR_LIST_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_LIST_MODEL, ThunarListModelClass))
#define THUNAR_IS_LIST_MODEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_LIST_MODEL))
#define THUNAR_IS_LIST_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_LIST_MODEL))
#define THUNAR_LIST_MODEL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_LIST_MODEL, ThunarListModelClass))

GType
thunar_list_model_get_type (void) G_GNUC_CONST;

ThunarStandardViewModel *
thunar_list_model_new (void);

void
thunar_list_model_check_file_in_model_before_use (ThunarListModel *model);


G_END_DECLS;

#endif /* !__THUNAR_LIST_MODEL_H__ */
