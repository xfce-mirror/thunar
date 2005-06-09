/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_FAVOURITES_MODEL_H__
#define __THUNAR_FAVOURITES_MODEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarFavouritesModelClass ThunarFavouritesModelClass;
typedef struct _ThunarFavouritesModel      ThunarFavouritesModel;

#define THUNAR_TYPE_FAVOURITES_MODEL            (thunar_favourites_model_get_type ())
#define THUNAR_FAVOURITES_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FAVOURITES_MODEL, ThunarFavouritesModel))
#define THUNAR_FAVOURITES_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FAVOURITES_MODEL, ThunarFavouritesModelClass))
#define THUNAR_IS_FAVOURITES_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FAVOURITES_MODEL))
#define THUNAR_IS_FAVOURITES_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FAVOURITES_MODEL))
#define THUNAR_FAVOURITES_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_MODEL_FAVOURITES_MODEL, ThunarFavouritesModelClass))

/**
 * ThunarFavouritesModelColumn:
 *
 * Columns exported by #ThunarFavouritesModel using the
 * #GtkTreeModel interface.
 **/
typedef enum
{
  THUNAR_FAVOURITES_MODEL_COLUMN_NAME,
  THUNAR_FAVOURITES_MODEL_COLUMN_ICON,
  THUNAR_FAVOURITES_MODEL_COLUMN_SEPARATOR,
  THUNAR_FAVOURITES_MODEL_N_COLUMNS,
} ThunarFavouritesModelColumn;

GType                  thunar_favourites_model_get_type       (void) G_GNUC_CONST;

ThunarFavouritesModel *thunar_favourites_model_get_default    (void);

gboolean               thunar_favourites_model_iter_for_file  (ThunarFavouritesModel *model,
                                                               ThunarFile            *file,
                                                               GtkTreeIter           *iter);
ThunarFile            *thunar_favourites_model_file_for_iter  (ThunarFavouritesModel *model,
                                                               GtkTreeIter           *iter);

gboolean               thunar_favourites_model_drop_possible  (ThunarFavouritesModel *model,
                                                               GtkTreePath           *path);

void                   thunar_favourites_model_move           (ThunarFavouritesModel *model,
                                                               GtkTreePath           *src_path,
                                                               GtkTreePath           *dst_path);

G_END_DECLS;

#endif /* !__THUNAR_FAVOURITES_MODEL_H__ */
