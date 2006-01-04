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

#ifndef __THUNAR_PERMISSIONS_MODEL_H__
#define __THUNAR_PERMISSIONS_MODEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarPermissionsModelClass ThunarPermissionsModelClass;
typedef struct _ThunarPermissionsModel      ThunarPermissionsModel;

#define THUNAR_TYPE_PERMISSIONS_MODEL             (thunar_permissions_model_get_type ())
#define THUNAR_PERMISSIONS_MODEL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PERMISSIONS_MODEL, ThunarPermissionsModel))
#define THUNAR_PERMISSIONS_MODEL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PERMISSIONS_MODEL, ThunarPermissionsModelClass))
#define THUNAR_IS_PERMISSIONS_MODEL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PERMISSIONS_MODEL))
#define THUNAR_IS_PERMISSIONS_MODEL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PERMISSIONS_MODEL))
#define THUNAR_PERMISSIONS_MODEL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PERMISSIONS_MODEL, ThunarPermissionsModelClass))

/**
 * ThunarPermissionsModelColumn:
 *
 * Columns provided by the #ThunarPermissionsModel class.
 **/
typedef enum
{
  THUNAR_PERMISSIONS_MODEL_COLUMN_TITLE,
  THUNAR_PERMISSIONS_MODEL_COLUMN_ICON,
  THUNAR_PERMISSIONS_MODEL_COLUMN_ICON_VISIBLE,
  THUNAR_PERMISSIONS_MODEL_COLUMN_STATE,
  THUNAR_PERMISSIONS_MODEL_COLUMN_STATE_VISIBLE,
  THUNAR_PERMISSIONS_MODEL_COLUMN_MUTABLE,
  THUNAR_PERMISSIONS_MODEL_N_COLUMNS,
} ThunarPermissionsModelColumn;

GType         thunar_permissions_model_get_type     (void) G_GNUC_CONST;

GtkTreeModel *thunar_permissions_model_new          (void) G_GNUC_MALLOC;

ThunarFile   *thunar_permissions_model_get_file     (ThunarPermissionsModel *model);
void          thunar_permissions_model_set_file     (ThunarPermissionsModel *model,
                                                     ThunarFile             *file);

gboolean      thunar_permissions_model_is_mutable   (ThunarPermissionsModel *model);

GList        *thunar_permissions_model_get_actions  (ThunarPermissionsModel *model,
                                                     GtkWidget              *widget,
                                                     GtkTreeIter            *iter) G_GNUC_MALLOC;

gboolean      thunar_permissions_model_toggle       (ThunarPermissionsModel *model,
                                                     GtkTreeIter            *iter,
                                                     GError                **error);

G_END_DECLS;

#endif /* !__THUNAR_PERMISSIONS_MODEL_H__ */
